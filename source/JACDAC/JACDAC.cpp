#include "JACDAC.h"
#include "Event.h"
#include "EventModel.h"
#include "codal_target_hal.h"
#include "CodalDmesg.h"
#include "CodalFiber.h"
#include "SingleWireSerial.h"
#include "Timer.h"

#define MAXIMUM_INTERBYTE_CC        0
#define MAXIMUM_LO_DATA_CC          0 // reuse the above channel
#define MINIMUM_INTERFRAME_CC       1

#define TIMER_CHANNELS_REQUIRED     2

#define JD_MAGIC_BUFFER_VALUE       0x1a

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

#define CODAL_ASSERT(cond)                                                                         \
    if (!(cond))                                                                                   \
    target_panic(0x5AC)

using namespace codal;

uint32_t error_count = 0;
JACDAC* JACDAC::instance = NULL;

struct BaudByte
{
    uint32_t baud;
    uint32_t time_per_byte;
};

/**
 * A simple look up for getting the time per byte given a baud rate.
 *
 * JDBaudRate index - 1 should be used to index this array.
 *
 * i.e. baudToByteMap[(uint8_t)Baud1M - 1].baud = 1000000
 **/
const BaudByte baudToByteMap[] =
{
    {1000000, 10},
    {500000, 20},
    {0, 0},
    {250000, 40},
    {0, 0},
    {0, 0},
    {0, 0},
    {125000, 80},
};

// https://graphics.stanford.edu/~seander/bithacks.html
// <3
inline uint32_t ceil_pow2(uint32_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

/**
 * Fletchers algorithm, a widely used low-overhead checksum (even used in apple latest file system APFS).
 *
 * @param data the data to checksum
 *
 * @param len the length of data
 **/
uint16_t fletcher16(const uint8_t *data, size_t len)
{
    uint32_t c0, c1;
    unsigned int i;

    for (c0 = c1 = 0; len >= 5802; len -= 5802)
    {
        for (i = 0; i < 5802; ++i)
        {
                c0 = c0 + *data++;
                c1 = c1 + c0;
        }
        c0 = c0 % 255;
        c1 = c1 % 255;
    }

    for (i = 0; i < len; ++i)
    {
        c0 = c0 + *data++;
        c1 = c1 + c0;
    }

    c0 = c0 % 255;
    c1 = c1 % 255;

    return (c1 << 8 | c0);
}
void jacdac_gpio_irq(int state)
{
    if (JACDAC::instance)
        JACDAC::instance->_gpioCallback(state);
}


void jacdac_timer_irq(uint16_t channels)
{
    if (JACDAC::instance)
        JACDAC::instance->_timerCallback(channels);
}

extern void set_gpio(int);
extern void set_gpio2(int);
void JACDAC::_gpioCallback(int state)
{
    // DMESG("%d",state);

    if (status & JD_SERIAL_ERR_MSK)
    {
        // TODO: if we're in error mode, just set start time on every toggle, callback in 80 us for errorState?
        // set_gpio2(1);
        bool toggle = false;
        if (state)
        {
            // Lo to hi transition?
            if (!(status & JD_SERIAL_BUS_STATE))
                toggle = true;

            status |= (JD_SERIAL_BUS_STATE | ((toggle) ? JD_SERIAL_BUS_TOGGLED : 0));
        }
        else
        {
            // Hi to lo transition?
            if (status & JD_SERIAL_BUS_STATE)
                toggle = true;

            status &= ~JD_SERIAL_BUS_STATE;
            status |= (toggle) ? JD_SERIAL_BUS_TOGGLED : 0;
        }
        // set_gpio2(0);
    }
    else
    {
        if (!state)
        {
            set_gpio(1);
            status |= JD_SERIAL_LO_PULSE_START;
            startTime = timer.captureCounter();
            timer.setCompare(MAXIMUM_INTERBYTE_CC, timer.captureCounter() + baudToByteMap[(uint8_t)JDBaudRate::Baud125K - 1].time_per_byte);
            set_gpio(0);
        }
        else if (status & JD_SERIAL_LO_PULSE_START)
        {
            set_gpio2(1);
            status &= ~JD_SERIAL_LO_PULSE_START;
            timer.clearCompare(MAXIMUM_INTERBYTE_CC);
            uint32_t end = timer.captureCounter();
            loPulseDetected((end > startTime) ? end - startTime : startTime - end);
            set_gpio2(0);
        }
    }
}

void JACDAC::_timerCallback(uint16_t channels)
{
    if (status & (JD_SERIAL_BUS_TIMEOUT_ERROR | JD_SERIAL_BUS_LO_ERROR | JD_SERIAL_BUS_UART_ERROR))
    {
        DMESG("CONT ERR");
        errorState(JDBusErrorState::Continuation);
        return;
    }

    if (channels & (1 << MAXIMUM_INTERBYTE_CC))
    {
        if (status & JD_SERIAL_LO_PULSE_START)
        {
            status &= ~JD_SERIAL_LO_PULSE_START;
            DMESG("BL ERR");
            errorState(JDBusErrorState::BusLoError);
            return;
        }
        else if (status & JD_SERIAL_RECEIVING_HEADER)
        {
            uint32_t endTime = timer.captureCounter();

            // if we've received data, swap to checking the framing of individual bytes.
            if (sws.getBytesReceived() > 0)
                timer.setCompare(MAXIMUM_LO_DATA_CC, endTime + JD_MAX_INTERBYTE_SPACING);

            // the maximum lo -> data spacing has been exceeded
            // enter the error state.
            else if (endTime - startTime >= JD_MAX_INTERLODATA_SPACING)
            {
                DMESG("BTO ERR");
                errorState(JDBusErrorState::BusTimeoutError);
            }

            // no data has been received and we haven't yet exceed lo -> data spacing
            else
            {
                startTime = endTime;
                timer.setCompare(MAXIMUM_LO_DATA_CC, endTime + JD_MAX_INTERBYTE_SPACING);
            }

            return;
        }
        else if (status & JD_SERIAL_RECEIVING)
        {
            uint16_t buffered = sws.getBytesReceived();

            if (buffered == lastBufferedCount)
            {
                DMESG("BTO1 ERR");
                errorState(JDBusErrorState::BusTimeoutError);
            }
            else
            {
                lastBufferedCount = buffered;
                // 2x interbyte, because it would be overkill to check every 160 us
                timer.setCompare(MAXIMUM_INTERBYTE_CC, timer.captureCounter() +  (2 * JD_MAX_INTERBYTE_SPACING));
            }
        }
    }

    if (channels & (1 << MINIMUM_INTERFRAME_CC))
        sendPacket();
}

void JACDAC::errorState(JDBusErrorState es)
{
    DMESG("ERROR! %d",es);
    // first time entering the error state?
    if (es != JDBusErrorState::Continuation && !(status & es))
    {
        set_gpio(1);
        DMESG("FIRST");
        status &= ~(JD_SERIAL_BUS_STATE | JD_SERIAL_BUS_TOGGLED);

        if (es == JD_SERIAL_BUS_TIMEOUT_ERROR || es == JD_SERIAL_BUS_UART_ERROR)
        {
            error_count++;
            sws.abortDMA();
            sws.setMode(SingleWireDisconnected);

            if (commLED)
                commLED->setDigitalValue(0);
        }

        status |= es;

        setState(JDSerialState::ErrorRecovery);

        Event(this->id, JD_SERIAL_EVT_BUS_ERROR);

        startTime = timer.captureCounter();
        timer.setCompare(MAXIMUM_INTERBYTE_CC, startTime + JD_BYTE_AT_125KBAUD);

        if (busLED)
            busLED->setDigitalValue(0);
        return;
    }

    // in error mode we detect if there is activity on the bus (we do this by flagging when the bus is toggled,
    // until the bus becomes idle defined by a period of JD_BUS_NORMALITY_PERIOD where there is no toggling)

    set_gpio2(1);
    // if the bus has not been toggled...
    if (!(status & JD_SERIAL_BUS_TOGGLED) && sp.getDigitalValue(PullMode::Up))
    {
        // set_gpio(1);
        uint32_t endTime = timer.captureCounter();

        if (endTime - startTime >= JD_BUS_NORMALITY_PERIOD)
        {
            if (busLED)
                busLED->setDigitalValue(1);

            status &= ~(JD_SERIAL_ERR_MSK);//~(JD_SERIAL_BUS_TIMEOUT_ERROR | JD_SERIAL_BUS_LO_ERROR | JD_SERIAL_BUS_UART_ERROR);

            // resume normality
            setState(JDSerialState::ListeningForPulse);
            set_gpio(0);

            // setup tx interrupt
            timer.setCompare(MINIMUM_INTERFRAME_CC, timer.captureCounter() + (JD_MIN_INTERFRAME_SPACING + target_random(JD_SERIAL_TX_MAX_BACKOFF)));
            return;
        }
        // set_gpio(0);
    }
    else
    {
        // set_gpio2(1);
        startTime = timer.captureCounter();
        // set_gpio2(0);
    }

    set_gpio2(0);
    // unset the bus toggled flag...
    status &= ~JD_SERIAL_BUS_TOGGLED;

    // otherwise come back in JD_BYTE_AT_125KBAUD us.
    timer.setCompare(MAXIMUM_INTERBYTE_CC, timer.captureCounter() + JD_BYTE_AT_125KBAUD);
}

void JACDAC::dmaComplete(Event evt)
{
    if (evt.value == SWS_EVT_ERROR)
    {
        timer.clearCompare(MAXIMUM_INTERBYTE_CC);

        error_count++;
        status &= ~(JD_SERIAL_RECEIVING | JD_SERIAL_RECEIVING_HEADER | JD_SERIAL_TRANSMITTING);
        DMESG("BUART ERR");
        errorState(JDBusErrorState::BusUARTError);
        return;
    }
    else
    {
        // rx complete, queue packet for later handling
        if (evt.value == SWS_EVT_DATA_RECEIVED)
        {
            if (status & JD_SERIAL_RECEIVING_HEADER)
            {
                status &= ~(JD_SERIAL_RECEIVING_HEADER);

                if (rxBuf->size)
                {
                    sws.receiveDMA(((uint8_t*)rxBuf) + JD_SERIAL_HEADER_SIZE, rxBuf->size);
                    timer.setCompare(MAXIMUM_INTERBYTE_CC, timer.captureCounter() + JD_MAX_INTERBYTE_SPACING);

                    status |= JD_SERIAL_RECEIVING;
                }

                JD_DMESG("H %d %d %d %d ", rxBuf->jacdac_version, rxBuf->crc, rxBuf->address, rxBuf->size);
                return;
            }
            else if (status & JD_SERIAL_RECEIVING)
            {
                status &= ~(JD_SERIAL_RECEIVING);
                timer.clearCompare(MAXIMUM_INTERBYTE_CC);

                uint8_t* crcPointer = (uint8_t*)&rxBuf->address;
                uint16_t crc = fletcher16(crcPointer, rxBuf->size + JD_SERIAL_CRC_HEADER_SIZE); // include size and address in the checksum.

                if (crc == rxBuf->crc && rxBuf->jacdac_version == JD_VERSION)
                {
                    rxBuf->communication_rate = (uint8_t)currentBaud;

                    // move rxbuf to rxArray and allocate new buffer.
                    addToRxArray(rxBuf);
                    rxBuf = (JDPacket*)malloc(sizeof(JDPacket));
                    Event(id, JD_SERIAL_EVT_DATA_READY);
                    DMESG("DMA RXD");
                }
                // could we do something cool to indicate an incorrect CRC?
                // i.e. drive the bus low....?
                else
                {
                    JD_DMESG("CRCE: %d, comp: %d",rxBuf->crc, crc);
                    Event(this->id, JD_SERIAL_EVT_CRC_ERROR);
                }
            }
        }

        if (evt.value == SWS_EVT_DATA_SENT)
        {
            status &= ~(JD_SERIAL_TRANSMITTING);
            free(txBuf);
            txBuf = NULL;
            DMESG("DMA TXD");
        }
    }

    sws.setMode(SingleWireDisconnected);

    // force transition to output so that the pin is reconfigured.
    // also drive the bus high for a little bit.
    sp.setDigitalValue(1);
    setState(JDSerialState::ListeningForPulse);

    timer.setCompare(MINIMUM_INTERFRAME_CC, timer.captureCounter() + (JD_MIN_INTERFRAME_SPACING + target_random(JD_SERIAL_TX_MAX_BACKOFF)));

    if (commLED)
        commLED->setDigitalValue(0);
}

void JACDAC::loPulseDetected(uint32_t pulseTime)
{
    // JD_DMESG("LO: %d %d", (status & JD_SERIAL_RECEIVING) ? 1 : 0, (status & JD_SERIAL_TRANSMITTING) ? 1 : 0);
    // guard against repeat events.
    if (status & (JD_SERIAL_RECEIVING | JD_SERIAL_RECEIVING_HEADER | JD_SERIAL_TRANSMITTING) || !(status & DEVICE_COMPONENT_RUNNING))
        return;

    DMESG("TS: %d", pulseTime);

    pulseTime = ceil(pulseTime / 10);
    pulseTime = ceil_pow2(pulseTime);

    JD_DMESG("TS: %d", pulseTime);

    // we support 1, 2, 4, 8 as our powers of 2.
    if (pulseTime > 8)
        return;

    // if zero round to 1 (to prevent div by 0)
    // it is assumed that the transaction is at 1 mbaud
    if (pulseTime == 0)
        pulseTime = 1;

    if (pulseTime < (uint8_t)this->maxBaud)
    {
        DMESG("BAUD ERR");
        // we can't receive at this baud rate
        errorState(JDBusErrorState::BusUARTError);
        return;
    }

    // 1 us to here

    if ((JDBaudRate)pulseTime != this->currentBaud)
    {
        sws.setBaud(baudToByteMap[pulseTime - 1].baud);
        this->currentBaud = (JDBaudRate)pulseTime;
        JD_DMESG("SB: %d",baudToByteMap[pulseTime - 1].baud);
    }

    // 1 more us
    sp.eventOn(DEVICE_PIN_EVENT_NONE);
    sp.getDigitalValue(PullMode::None);

    // 10 us
    status |= (JD_SERIAL_RECEIVING_HEADER);

    bufferOffset = 0;
    sws.receiveDMA((uint8_t*)rxBuf, JD_SERIAL_HEADER_SIZE);
    // 14 more us
    startTime = timer.captureCounter();
    timer.setCompare(MAXIMUM_LO_DATA_CC, startTime + JD_MAX_INTERLODATA_SPACING);

    if (commLED)
        commLED->setDigitalValue(1);
}

void JACDAC::setState(JDSerialState state)
{
    sp.getDigitalValue(PullMode::Up);

    uint32_t eventType = 0;

    // to ensure atomicity of the state machine, we disable one set of event and enable the other.
    if (state == JDSerialState::ListeningForPulse)
    {
        eventType = DEVICE_PIN_INTERRUPT_ON_EDGE;
    }

    if (state == JDSerialState::ErrorRecovery)
    {
        eventType = DEVICE_PIN_INTERRUPT_ON_EDGE;
    }

    if (state == JDSerialState::Off)
    {
        eventType = DEVICE_PIN_EVENT_NONE;
    }

    this->state = state;

    sp.eventOn(eventType);
}

void JACDAC::initialise()
{
    rxBuf = NULL;
    txBuf = NULL;
    memset(rxArray, 0, sizeof(JDPacket*) * JD_RX_ARRAY_SIZE);
    memset(txArray, 0, sizeof(JDPacket*) * JD_TX_ARRAY_SIZE);

    status = 0;

    // 32 bit 1 us timer.
    timer.setBitMode(BitMode32);
    timer.setClockSpeed(1000);
    timer.setIRQ(jacdac_timer_irq);
    timer.enable();

    sp.setIRQ(jacdac_gpio_irq);
    sp.getDigitalValue(PullMode::None);

    sws.setDMACompletionHandler(this, &JACDAC::dmaComplete);
}

/**
 * Constructor
 *
 * @param p the transmission pin to use
 *
 * @param sws an instance of sws created using p.
 */
JACDAC::JACDAC(DMASingleWireSerial&  sws, LowLevelTimer& timer, Pin* busLED, Pin* commStateLED, JDBaudRate maxBaudRate, uint16_t id) : sws(sws), sp(sws.p), timer(timer), busLED(busLED), commLED(commStateLED)
{
    this->id = id;
    this->maxBaud = maxBaudRate;
    instance = this;

    // at least three channels are required.
    CODAL_ASSERT(timer.getChannelCount() >= TIMER_CHANNELS_REQUIRED);

    initialise();
}

/**
 * Retrieves the first packet on the rxQueue irregardless of the device_class
 *
 * @returns the first packet on the rxQueue or NULL
 */
JDPacket* JACDAC::getPacket()
{
    return popRxArray();
}

/**
 * Causes this instance of JACDAC to begin listening for packets transmitted on the serial line.
 */
void JACDAC::start()
{
    if (isRunning())
        return;

    if (rxBuf == NULL)
        rxBuf = (JDPacket*)malloc(sizeof(JDPacket));

    DMESG("JD START");


    status |= DEVICE_COMPONENT_RUNNING;

    // check if the bus is lo here and change our led
    setState(JDSerialState::ListeningForPulse);

    if (busLED)
        busLED->setDigitalValue(1);

    Event(this->id, JD_SERIAL_EVT_BUS_CONNECTED);
}

/**
 * Causes this instance of JACDAC to stop listening for packets transmitted on the serial line.
 */
void JACDAC::stop()
{
    if (!isRunning())
        return;

    status &= ~(DEVICE_COMPONENT_RUNNING);
    if (rxBuf)
    {
        free(rxBuf);
        rxBuf = NULL;
    }

    setState(JDSerialState::Off);

    if (busLED)
        busLED->setDigitalValue(0);

    Event(this->id, JD_SERIAL_EVT_BUS_DISCONNECTED);
}

void JACDAC::sendPacket()
{
    DMESG("SENDP");
    // if we are receiving, the tx timer will be resumed upon reception.
    if (status & (JD_SERIAL_RECEIVING | JD_SERIAL_RECEIVING_HEADER))
    {
        JD_DMESG("RXing ret");
        return;
    }
    if (!(status & JD_SERIAL_TRANSMITTING))
    {
        // if the bus is lo, we shouldn't transmit
        if (sp.getDigitalValue(PullMode::Up) == 0)
        {
            JD_DMESG("BUS LO");

            if (busLED)
                busLED->setDigitalValue(0);

            if (commLED)
                commLED->setDigitalValue(0);

            DMESG("TXLO ERR");
            errorState(JDBusErrorState::BusLoError);
            return;
        }

        // If we get here, we assume we have control of the bus.
        // check if we have stuff to send, txBuf will be set if a previous send failed.
        if (this->txHead != this->txTail || txBuf)
        {
            // JD_DMESG("TX B");
            status |= JD_SERIAL_TRANSMITTING;

            // we may have a packet that we previously tried to send, but was aborted for some reason.
            if (txBuf == NULL)
                txBuf = popTxArray();

            sp.setDigitalValue(0);
            target_disable_irq();
            target_wait_us(baudToByteMap[(uint8_t)txBuf->communication_rate - 1].time_per_byte);
            target_enable_irq();
            sp.setDigitalValue(1);

            if (txBuf->communication_rate != (uint8_t)currentBaud)
            {
                sws.setBaud(baudToByteMap[(uint8_t)txBuf->communication_rate - 1].baud);
                currentBaud = (JDBaudRate)txBuf->communication_rate;
            }

            // configure to come back after the minimum lo_data gap has been observed.
            timer.setCompare(MINIMUM_INTERFRAME_CC, timer.captureCounter() + (baudToByteMap[(uint8_t)txBuf->communication_rate - 1].time_per_byte * JD_INTERLODATA_SPACING_MULTIPLIER));
            return;
        }
    }

    // we've returned after a DMA transfer has been flagged (above)... start
    if (status & JD_SERIAL_TRANSMITTING)
    {
        // JD_DMESG("TX S");
        sws.sendDMA((uint8_t *)txBuf, txBuf->size + JD_SERIAL_HEADER_SIZE);
        if (commLED)
            commLED->setDigitalValue(1);
        return;
    }

    // if we get here, there's no more to transmit
    status &= ~(JD_SERIAL_TX_DRAIN_ENABLE);
    return;
}

/**
 * Sends a packet using the SingleWireSerial instance. This function begins the asynchronous transmission of a packet.
 * If an ongoing asynchronous transmission is happening, JD is added to the txQueue. If this is the first packet in a while
 * asynchronous transmission is begun.
 *
 * @param JD the packet to send.
 *
 * @returns DEVICE_OK on success, DEVICE_INVALID_PARAMETER if JD is NULL, or DEVICE_NO_RESOURCES if the queue is full.
 */
int JACDAC::send(JDPacket* tx, bool compute_crc)
{
    JD_DMESG("SEND");
    // if checks is not set, we skip error checking.
    if (tx == NULL)
        return DEVICE_INVALID_PARAMETER;

    // don't queue packets if jacdac is not running, or the bus is being held LO.
    if (!isRunning() || status & JD_SERIAL_BUS_LO_ERROR)
        return DEVICE_INVALID_STATE;

    JD_DMESG("QUEU");

    JDPacket* pkt = (JDPacket *)malloc(sizeof(JDPacket));
    memset(pkt, 0, sizeof(JDPacket));
    memcpy(pkt, tx, sizeof(JDPacket));

    if (pkt->communication_rate < (uint8_t) maxBaud)
        pkt->communication_rate = (uint8_t)maxBaud;

    JD_DMESG("QU %d", pkt->size);

    pkt->jacdac_version = JD_VERSION;

    // if compute_crc is not set, we assume the user is competent enough to use their own crc mechanisms.
    if (compute_crc)
    {
        // crc is calculated from the address field onwards
        uint8_t* crcPointer = (uint8_t*)&pkt->address;
        pkt->crc = fletcher16(crcPointer, pkt->size + JD_SERIAL_CRC_HEADER_SIZE);
    }

    int ret = addToTxArray(pkt);

    if (!(status & JD_SERIAL_TX_DRAIN_ENABLE))
    {
        DMESG("DR EN %d", timer.captureCounter());
        status |= JD_SERIAL_TX_DRAIN_ENABLE;
        int ret = timer.setCompare(MINIMUM_INTERFRAME_CC, timer.captureCounter() + (JD_MIN_INTERFRAME_SPACING + target_random(JD_SERIAL_TX_MAX_BACKOFF)));
        JD_DMESG("SC: %d",ret);
    }

    return ret;
}

/**
 * Sends a packet using the SingleWireSerial instance. This function begins the asynchronous transmission of a packet.
 * If an ongoing asynchronous transmission is happening, pkt is added to the txQueue. If this is the first packet in a while
 * asynchronous transmission is begun.
 *
 * @param buf the buffer to send.
 *
 * @param len the length of the buffer to send.
 *
 * @returns DEVICE_OK on success, DEVICE_INVALID_PARAMETER if buf is NULL or len is invalid, or DEVICE_NO_RESOURCES if the queue is full.
 */
int JACDAC::send(uint8_t* buf, int len, uint8_t address, JDBaudRate br)
{
    if (buf == NULL || len <= 0 || len > JD_SERIAL_MAX_PAYLOAD_SIZE)
    {
        JD_DMESG("pkt TOO BIG: %d ",len);
        return DEVICE_INVALID_PARAMETER;
    }

    JDPacket pkt;
    memset(&pkt, 0, sizeof(JDPacket));

    pkt.crc = 0;
    pkt.address = address;
    pkt.size = len;
    pkt.communication_rate = (uint8_t)br;

    memcpy(pkt.data, buf, len);

    return send(&pkt);
}

/**
 * Returns a bool indicating whether the JACDAC driver has been started.
 *
 * @return true if started, false if not.
 **/
bool JACDAC::isRunning()
{
    return (status & DEVICE_COMPONENT_RUNNING) ? true : false;
}

/**
 * Returns the current state if the bus.
 *
 * @return true if connected, false if there's a bad bus condition.
 **/
bool JACDAC::isConnected()
{
    if (status & JD_SERIAL_RECEIVING || (status & JD_SERIAL_TRANSMITTING && !(status & JD_SERIAL_BUS_LO_ERROR)))
        return true;

    // this flag is set if the bus is being held lo.
    if (status & JD_SERIAL_BUS_LO_ERROR)
        return false;

    // if we are neither transmitting or receiving, examine the bus.
    int busVal = sp.getDigitalValue(PullMode::Up);

    // re-enable events!
    setState(JDSerialState::ListeningForPulse);

    if (busVal)
        return true;

    return false;
}

/**
 * Returns the current state of the bus, either:
 *
 * * Receiving if the driver is in the process of receiving a packet.
 * * Transmitting if the driver is communicating a packet on the bus.
 *
 * If neither of the previous states are true, then the driver looks at the bus and returns the bus state:
 *
 * * High, if the line is currently floating high.
 * * Lo if something is currently pulling the line low.
 **/
JDBusState JACDAC::getState()
{
    if (status & (JD_SERIAL_RECEIVING | JD_SERIAL_RECEIVING_HEADER))
        return JDBusState::Receiving;

    if (status & JD_SERIAL_TRANSMITTING)
        return JDBusState::Transmitting;

    // this flag is set if the bus is being held lo.
    if (status & JD_SERIAL_BUS_LO_ERROR)
        return JDBusState::Low;

    // if we are neither transmitting or receiving, examine the bus.
    int busVal = sp.getDigitalValue(PullMode::Up);
    // re-enable events!
    setState(JDSerialState::ListeningForPulse);

    if (busVal)
        return JDBusState::High;

    return JDBusState::Low;
}


int JACDAC::setMaximumBaud(JDBaudRate baud)
{
    this->maxBaud = baud;
    return DEVICE_OK;
}

JDBaudRate JACDAC::getMaximumBaud()
{
    return this->maxBaud;
}

/**
 * Our arrays are FIFO circular buffers.
 *
 * Example:
 * txHead             txTail
 * [item, item, item, item, NULL, NULL, NULL]
 *
 * Remove:
 *        txHead      txTail
 * [NULL, item, item, item, NULL, NULL, NULL]
 *
 * Add:
 *        txHead            txTail
 * [NULL, item, item, item, item, NULL, NULL]
 **/
JDPacket* JACDAC::popRxArray()
{
    // nothing to pop
    if (this->rxTail == this->rxHead)
        return NULL;

    uint8_t nextHead = (this->rxHead + 1) % JD_RX_ARRAY_SIZE;
    JDPacket* p = rxArray[this->rxHead];
    this->rxArray[this->rxHead] = NULL;
    target_disable_irq();
    this->rxHead = nextHead;
    target_enable_irq();

    return p;
}

/**
 * Our arrays are FIFO circular buffers.
 *
 * Example:
 * txHead                   txTail
 * [item, item, item, item, NULL, NULL, NULL]
 *
 * Remove:
 *        txHead            txTail
 * [NULL, item, item, item, NULL, NULL, NULL]
 *
 * Add:
 *        txHead                  txTail
 * [NULL, item, item, item, item, NULL, NULL]
 **/
JDPacket* JACDAC::popTxArray()
{
    // nothing to pop
    if (this->txTail == this->txHead)
        return NULL;

    uint8_t nextHead = (this->txHead + 1) % JD_TX_ARRAY_SIZE;
    JDPacket* p = txArray[this->txHead];
    this->txArray[this->txHead] = NULL;
    target_disable_irq();
    this->txHead = nextHead;
    target_enable_irq();

    return p;
}

/**
 * Our arrays are FIFO circular buffers.
 *
 * Example:
 * txHead                   txTail
 * [item, item, item, item, NULL, NULL, NULL]
 *
 * Remove:
 *        txHead            txTail
 * [NULL, item, item, item, NULL, NULL, NULL]
 *
 * Add:
 *        txHead                  txTail
 * [NULL, item, item, item, item, NULL, NULL]
 **/
int JACDAC::addToTxArray(JDPacket* packet)
{
     uint8_t nextTail = (this->txTail + 1) % JD_TX_ARRAY_SIZE;

    if (nextTail == this->txHead)
        return DEVICE_NO_RESOURCES;

    // add our buffer to the array before updating the head
    // this ensures atomicity.
    this->txArray[this->txTail] = packet;
    target_disable_irq();
    this->txTail = nextTail;
    target_enable_irq();

    return DEVICE_OK;
}

/**
 * Our arrays are FIFO circular buffers.
 *
 * Example:
 * txHead                   txTail
 * [item, item, item, item, NULL, NULL, NULL]
 *
 * Remove:
 *        txHead            txTail
 * [NULL, item, item, item, NULL, NULL, NULL]
 *
 * Add:
 *        txHead                  txTail
 * [NULL, item, item, item, item, NULL, NULL]
 **/
int JACDAC::addToRxArray(JDPacket* packet)
{
     uint8_t nextTail = (this->rxTail + 1) % JD_RX_ARRAY_SIZE;

    if (nextTail == this->rxHead)
        return DEVICE_NO_RESOURCES;

    // add our buffer to the array before updating the head
    // this ensures atomicity.
    this->rxArray[this->rxTail] = packet;
    target_disable_irq();
    this->rxTail = nextTail;
    target_enable_irq();

    return DEVICE_OK;
}
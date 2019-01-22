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
    {250000, 40},
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

void jacdac_timer_irq(uint16_t channels)
{
    if (JACDAC::instance)
        JACDAC::instance->_timerCallback(channels);
}

void JACDAC::_timerCallback(uint16_t channels)
{
    if (status & (JD_SERIAL_BUS_TIMEOUT_ERROR | JD_SERIAL_BUS_LO_ERROR))
    {
        errorState(JDBusErrorState::Continuation);
        return;
    }

    if (channels & (1 << MAXIMUM_INTERBYTE_CC))
    {
        if (status & JD_SERIAL_RECEIVING_HEADER)
        {
            uint32_t endTime = timer.captureCounter();

            // if we've received data, swap to checking the framing of individual bytes.
            if (sws.getBytesReceived() > 0)
                timer.setCompare(MAXIMUM_LO_DATA_CC, endTime + JD_MAX_INTERBYTE_SPACING);

            // the maximum lo -> data spacing has been exceeded
            // enter the error state.
            else if (endTime - startTime >= baudToByteMap[(uint8_t)currentBaud - 1].time_per_byte * JD_INTERLODATA_SPACING_MULTIPLIER)
                errorState(JDBusErrorState::BusTimeoutError);

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
                errorState(JDBusErrorState::BusTimeoutError);
            else
            {
                lastBufferedCount = buffered;
                // 2x interbyte, because it would be overkill to check every 160 us
                timer.setCompare(MAXIMUM_INTERBYTE_CC, timer.captureCounter() +  (2 * JD_MAX_INTERBYTE_SPACING));
            }
        }
    }

    if (channels & (1 << MINIMUM_INTERFRAME_CC))
    {
        sendPacket();
    }
}

void JACDAC::dmaComplete(Event evt)
{
    if (evt.value == SWS_EVT_ERROR)
    {
        timer.clearCompare(MAXIMUM_INTERBYTE_CC);

        error_count++;
        status &= ~(JD_SERIAL_RECEIVING | JD_SERIAL_RECEIVING_HEADER | JD_SERIAL_TRANSMITTING);
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

                JDPacket* rx = (JDPacket*)rxBuf;
                sws.receiveDMA(((uint8_t*)rxBuf) + JD_SERIAL_HEADER_SIZE, rx->size);
                timer.setCompare(MAXIMUM_INTERBYTE_CC, timer.captureCounter() + (2 * JD_MAX_INTERBYTE_SPACING));
                JD_DMESG("RXH %d",rx->size);

                status |= JD_SERIAL_RECEIVING;
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
                    memset(rxBuf, JD_MAGIC_BUFFER_VALUE, sizeof(JDPacket));
                    Event(id, JD_SERIAL_EVT_DATA_READY);
                    JD_DMESG("DMA RXD");
                }
                // could we do something cool to indicate an incorrect CRC?
                // i.e. drive the bus low....?
                else
                {
                    JD_DMESG("CRCE: %d, comp: %d",rxBuf->crc, crc);
                    // uint8_t* bufPtr = (uint8_t*)rxBuf;
                    // for (int i = 0; i < JD_SERIAL_HEADER_SIZE + 2; i++)
                    //     JD_DMESG("%d[%c]",bufPtr[i]);
                }
            }
        }

        if (evt.value == SWS_EVT_DATA_SENT)
        {
            status &= ~(JD_SERIAL_TRANSMITTING);
            free(txBuf);
            txBuf = NULL;
            JD_DMESG("DMA TXD");
        }
    }

    sws.setMode(SingleWireDisconnected);

    // force transition to output so that the pin is reconfigured.
    // also drive the bus high for a little bit.
    sp.setDigitalValue(1);
    configure(JDPinEvents::ListeningForPulse);

    timer.setCompare(MINIMUM_INTERFRAME_CC, timer.captureCounter() + (JD_MIN_INTERFRAME_SPACING + target_random(JD_SERIAL_TX_MAX_BACKOFF)));

    if (commLED)
        commLED->setDigitalValue(0);
}

void JACDAC::onLowPulse(Event e)
{
    JD_DMESG("LO: %d %d", (status & JD_SERIAL_RECEIVING) ? 1 : 0, (status & JD_SERIAL_TRANSMITTING) ? 1 : 0);
    // guard against repeat events.
    if (status & (JD_SERIAL_RECEIVING | JD_SERIAL_RECEIVING_HEADER | JD_SERIAL_TRANSMITTING) || !(status & DEVICE_COMPONENT_RUNNING))
        return;

    uint32_t ts = e.timestamp;
    ts = ceil(ts / 10);
    ts = ceil_pow2(ts);

    JD_DMESG("TS: %d %d", ts, (int)e.timestamp);

    // we support 1, 2, 4, 8 as our powers of 2.
    if (ts > 8)
        return;

    // if zero round to 1 (to prevent div by 0)
    // it is assumed that the transaction is at 1 mbaud
    if (ts == 0)
        ts = 1;

    if (ts < (uint8_t)this->maxBaud)
        // we can't receive at this baud rate
        errorState(JDBusErrorState::BusUARTError);

    if ((JDBaudRate)ts != this->currentBaud)
    {
        sws.setBaud(baudToByteMap[ts - 1].baud);
        this->currentBaud = (JDBaudRate)ts;
    }

    sp.eventOn(DEVICE_PIN_EVENT_NONE);
    sp.getDigitalValue(PullMode::None);

    status |= (JD_SERIAL_RECEIVING_HEADER);

    bufferOffset = 0;
    sws.receiveDMA((uint8_t*)rxBuf, JD_SERIAL_HEADER_SIZE);

    startTime = timer.captureCounter();
    timer.setCompare(MAXIMUM_LO_DATA_CC, baudToByteMap[(uint8_t)currentBaud - 1].time_per_byte * JD_INTERLODATA_SPACING_MULTIPLIER);

    if (commLED)
        commLED->setDigitalValue(1);
}

void JACDAC::configure(JDPinEvents eventType)
{
    sp.getDigitalValue(PullMode::Up);

    // to ensure atomicity of the state machine, we disable one set of event and enable the other.
    if (eventType == ListeningForPulse)
    {
        EventModel::defaultEventBus->listen(sp.id, DEVICE_PIN_EVT_PULSE_LO, this, &JACDAC::onLowPulse, MESSAGE_BUS_LISTENER_IMMEDIATE);
        EventModel::defaultEventBus->ignore(sp.id, DEVICE_PIN_EVT_RISE, this, &JACDAC::onRiseFall);
        EventModel::defaultEventBus->ignore(sp.id, DEVICE_PIN_EVT_FALL, this, &JACDAC::onRiseFall);
    }

    if (eventType == DetectBusEdge)
    {
        EventModel::defaultEventBus->listen(sp.id, DEVICE_PIN_EVT_RISE, this, &JACDAC::onRiseFall, MESSAGE_BUS_LISTENER_IMMEDIATE);
        EventModel::defaultEventBus->listen(sp.id, DEVICE_PIN_EVT_FALL, this, &JACDAC::onRiseFall, MESSAGE_BUS_LISTENER_IMMEDIATE);
        EventModel::defaultEventBus->ignore(sp.id, DEVICE_PIN_EVT_PULSE_LO, this, &JACDAC::onLowPulse);
    }

    if (eventType == Off)
    {
        // remove all active listeners too
        EventModel::defaultEventBus->ignore(sp.id, DEVICE_PIN_EVT_PULSE_LO, this, &JACDAC::onLowPulse);
        EventModel::defaultEventBus->ignore(sp.id, DEVICE_PIN_EVT_RISE, this, &JACDAC::onRiseFall);
        EventModel::defaultEventBus->ignore(sp.id, DEVICE_PIN_EVT_FALL, this, &JACDAC::onRiseFall);
    }

    sp.eventOn(eventType);
}

void JACDAC::initialise()
{
    rxBuf = NULL;
    txBuf = NULL;
    memset(rxArray, 0, sizeof(JDPacket*) * JD_RX_ARRAY_SIZE);
    memset(txArray, 0, sizeof(JDPacket*) * JD_TX_ARRAY_SIZE);

    this->id = id;
    status = 0;

    // 32 bit 1 us timer.
    timer.setBitMode(BitMode32);
    timer.setClockSpeed(1000);
    timer.setIRQ(jacdac_timer_irq);
    timer.enable();

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

    JD_DMESG("JD START");

    status = 0;
    status |= DEVICE_COMPONENT_RUNNING;

    // check if the bus is lo here and change our led
    configure(JDPinEvents::ListeningForPulse);

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

    configure(JDPinEvents::Off);

    if (busLED)
        busLED->setDigitalValue(0);
    Event(this->id, JD_SERIAL_EVT_BUS_DISCONNECTED);
}

void JACDAC::onRiseFall(Event e)
{
    bool toggle = false;
    if (e.value == DEVICE_PIN_EVT_RISE)
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
}

void JACDAC::errorState(JDBusErrorState es)
{
    // first time entering the error state?
    if (es != JDBusErrorState::Continuation && !(status & es))
    {
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

        configure(JDPinEvents::DetectBusEdge);

        Event(this->id, JD_SERIAL_EVT_BUS_ERROR);

        startTime = timer.captureCounter();
        timer.setCompare(MAXIMUM_INTERBYTE_CC, startTime + JD_MAX_INTERBYTE_SPACING);
        return;
    }

    // in error mode we detect if there is activity on the bus (we do this by flagging when the bus is toggled,
    // until the bus becomes idle defined by a period of JD_BUS_NORMALITY_PERIOD where there is no toggling)

    // if the bus has not been toggled...
    if (!(status & JD_SERIAL_BUS_TOGGLED))
    {
        uint32_t endTime = timer.captureCounter();

        if (endTime - startTime >= JD_BUS_NORMALITY_PERIOD && sp.getDigitalValue(PullMode::Up))
        {
            if (status & JD_SERIAL_BUS_LO_ERROR && busLED)
                busLED->setDigitalValue(1);

            status &= ~(JD_SERIAL_BUS_TIMEOUT_ERROR | JD_SERIAL_BUS_LO_ERROR);

            // resume normality
            configure(JDPinEvents::ListeningForPulse);

            timer.setCompare(MINIMUM_INTERFRAME_CC,  + (JD_MIN_INTERFRAME_SPACING + target_random(JD_SERIAL_TX_MAX_BACKOFF)));
            return;
        }
    }

    // otherwise come back in JD_MAX_INTERBYTE_SPACING us.
    startTime = timer.captureCounter();
    timer.setCompare(MAXIMUM_INTERBYTE_CC, startTime + JD_MAX_INTERBYTE_SPACING);
}

void JACDAC::sendPacket()
{
    JD_DMESG("SENDP");
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

            errorState(JDBusErrorState::BusLoError);
            return;
        }

        // If we get here, we assume we have control of the bus.
        // check if we have stuff to send, txBuf will be set if a previous send failed.
        if (this->txHead != this->txTail || txBuf)
        {
            JD_DMESG("TX B");
            status |= JD_SERIAL_TRANSMITTING;

            // we may have a packet that we previously tried to send, but was aborted for some reason.
            if (txBuf == NULL)
                txBuf = popTxArray();

            sp.setDigitalValue(0);
            target_wait_us(baudToByteMap[(uint8_t)txBuf->communication_rate - 1].time_per_byte);
            sp.setDigitalValue(1);

            if (txBuf->communication_rate != (uint8_t)currentBaud)
            {
                sws.setBaud(baudToByteMap[(uint8_t)txBuf->communication_rate - 1].baud);
                currentBaud = (JDBaudRate)txBuf->communication_rate;
            }
        }
        JD_DMESG("txh: %d txt: %d",txHead,txTail);
    }

    // we've returned after a DMA transfer has been flagged (above)... start
    if (status & JD_SERIAL_TRANSMITTING)
    {
        JD_DMESG("TX S");
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
        JD_DMESG("DR EN");
        status |= JD_SERIAL_TX_DRAIN_ENABLE;
        timer.setCompare(MINIMUM_INTERFRAME_CC, timer.captureCounter() + (JD_MIN_INTERFRAME_SPACING + target_random(JD_SERIAL_TX_MAX_BACKOFF)));
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
    configure(JDPinEvents::ListeningForPulse);

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
    if (status & JD_SERIAL_RECEIVING)
        return JDBusState::Receiving;

    if (status & JD_SERIAL_TRANSMITTING)
        return JDBusState::Transmitting;

    // this flag is set if the bus is being held lo.
    if (status & JD_SERIAL_BUS_LO_ERROR)
        return JDBusState::Low;

    // if we are neither transmitting or receiving, examine the bus.
    int busVal = sp.getDigitalValue(PullMode::Up);
    // re-enable events!
    configure(JDPinEvents::ListeningForPulse);

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
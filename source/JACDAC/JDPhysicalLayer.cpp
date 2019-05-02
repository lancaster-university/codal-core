#include "JDPhysicalLayer.h"
#include "Event.h"
#include "EventModel.h"
#include "codal_target_hal.h"
#include "CodalDmesg.h"
#include "CodalFiber.h"
#include "SingleWireSerial.h"
#include "Timer.h"
#include "JACDAC.h"
#include "JDCRC.h"

#define TRACK_STATE

#ifdef TRACK_STATE

// #define PHYS_STATE_SIZE         128
#define PHYS_STATE_SIZE         512

#warning TRACK_STATE_ON

uint32_t phys_state[PHYS_STATE_SIZE] = {0};
uint32_t phys_pointer = 0;

#define JD_SET_FLAGS(flags,num) do {                        \
                            test_status |= (flags);     \
                            phys_state[phys_pointer] = test_status | (__LINE__ << 16) | 1 << 31;  \
                            phys_pointer = (phys_pointer + 1) % PHYS_STATE_SIZE;    \
                            }while(0)

#define JD_UNSET_FLAGS(flags,num) do {                          \
                                test_status &= ~(flags);    \
                                phys_state[phys_pointer] = test_status | (__LINE__ << 16); \
                                phys_pointer = (phys_pointer + 1) % PHYS_STATE_SIZE;    \
                              }while(0)

#else
#define JD_SET_FLAGS(...) ((void)0)
#define JD_UNSET_FLAGS(...) ((void)0)
#endif

#define MAXIMUM_INTERBYTE_CC        0
#define MAXIMUM_LO_DATA_CC          0 // reuse the above channel
#define MINIMUM_INTERFRAME_CC       1

#define TIMER_CHANNELS_REQUIRED     2

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

#define COMM_LED_LO ((this->commLEDActiveLo) ? 1 : 0)
#define COMM_LED_HI ((this->commLEDActiveLo) ? 0 : 1)

#define BUS_LED_LO ((this->busLEDActiveLo) ? 1 : 0)
#define BUS_LED_HI ((this->busLEDActiveLo) ? 0 : 1)

using namespace codal;

volatile uint32_t test_status = 0;

JDDiagnostics diagnostics;
JDPhysicalLayer* JDPhysicalLayer::instance = NULL;

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

extern "C" void set_gpio(int);
void jacdac_gpio_irq(int state)
{
    if (JDPhysicalLayer::instance)
        JDPhysicalLayer::instance->_gpioCallback(state);
}


void jacdac_timer_irq(uint16_t channels)
{
    if (JDPhysicalLayer::instance)
        JDPhysicalLayer::instance->_timerCallback(channels);
}

extern void set_gpio2(int);
extern void set_gpio3(int);
extern void set_gpio4(int);

void JDPhysicalLayer::_gpioCallback(int state)
{
    uint32_t now = timer.captureCounter();
    // set_gpio(0);

    if (state)
    {
        JD_SET_FLAGS(JD_SERIAL_BUS_STATE,0);

        if (test_status & JD_SERIAL_ERR_MSK)
        {
            // all flags
            CODAL_ASSERT(!(test_status & (JD_SERIAL_RECEIVING | JD_SERIAL_RECEIVING_HEADER | JD_SERIAL_TRANSMITTING)),test_status)
            startTime = now;
            timer.setCompare(MAXIMUM_INTERBYTE_CC, startTime + JD_BYTE_AT_125KBAUD);
            // set_gpio2(0);
            // DMESG("A %d",test_status);
        }
        else if (test_status & JD_SERIAL_LO_PULSE_START)
        {
            // set_gpio(1);
            CODAL_ASSERT(!(test_status & (JD_SERIAL_RECEIVING | JD_SERIAL_RECEIVING_HEADER | JD_SERIAL_TRANSMITTING)),test_status)
            JD_UNSET_FLAGS(JD_SERIAL_LO_PULSE_START,1);
            timer.clearCompare(MAXIMUM_INTERBYTE_CC);
            loPulseDetected((now > startTime) ? now - startTime : startTime - now);
        }
    }
    else
    {
        // set_gpio(1);
        JD_UNSET_FLAGS(JD_SERIAL_BUS_STATE,2);
        startTime = now;
        // set_gpio(0);
        if (!(test_status & JD_SERIAL_ERR_MSK))
        {
            // set_gpio2(1);
            JD_SET_FLAGS(JD_SERIAL_LO_PULSE_START,3);
            timer.setCompare(MAXIMUM_INTERBYTE_CC, startTime + baudToByteMap[(uint8_t)JDBaudRate::Baud125K - 1].time_per_byte);
            // set_gpio2(0);
        }
        else
        {
            // DMESG("B");
        }

    }
    // set_gpio(0);
}

void JDPhysicalLayer::errorState(JDBusErrorState es)
{
    JD_DMESG("ERROR! %d",es);
    // first time entering the error state?
    if (es != JDBusErrorState::Continuation)
    {
        JD_UNSET_FLAGS(JD_SERIAL_RECEIVING | JD_SERIAL_RECEIVING_HEADER | JD_SERIAL_LO_PULSE_START | JD_SERIAL_TRANSMITTING | JD_SERIAL_BUS_STATE, 4);

        if (es == JD_SERIAL_BUS_TIMEOUT_ERROR)
            diagnostics.bus_timeout_error++;

        if (es == JD_SERIAL_BUS_UART_ERROR)
            diagnostics.bus_uart_error++;

        if (es == JD_SERIAL_BUS_LO_ERROR)
            diagnostics.bus_lo_error++;

        // set_gpio3(1);
        // DMESG("ERR %d",es);
        if (es == JD_SERIAL_BUS_TIMEOUT_ERROR || es == JD_SERIAL_BUS_UART_ERROR)
        {
            sws.abortDMA();
            sws.setMode(SingleWireDisconnected);

            if (commLED)
                commLED->setDigitalValue(COMM_LED_LO);
        }

        // update the bus state before enabling our IRQ.
        JD_SET_FLAGS(es | (sp.getDigitalValue(PullMode::Up) ? JD_SERIAL_BUS_STATE : 0),5);
        // DMESG("EST %d",test_status);
        startTime = timer.captureCounter();

        timer.setCompare(MAXIMUM_INTERBYTE_CC, startTime + JD_BYTE_AT_125KBAUD);
        setState(JDSerialState::ErrorRecovery);

        // Event(this->id, JD_SERIAL_EVT_BUS_ERROR);

        if (busLED)
            busLED->setDigitalValue(BUS_LED_LO);

        // set_gpio3(0);
        // DMESG("OUT %d",test_status);
        return;
    }

    // in error mode we detect if there is activity on the bus (we do this by resetting start time when the bus is toggled,
    // until the bus becomes idle defined by a period of JD_BUS_NORMALITY_PERIOD)
    // set_gpio4(1);
    uint32_t endTime = timer.captureCounter();

    if (test_status & JD_SERIAL_BUS_STATE && endTime - startTime >= JD_BUS_NORMALITY_PERIOD)
    {
        if (busLED)
            busLED->setDigitalValue(BUS_LED_HI);
        // DMESG("B4 %d",test_status);
        JD_UNSET_FLAGS(JD_SERIAL_ERR_MSK,6);
        // resume normality
        setState(JDSerialState::ListeningForPulse);
        // set_gpio4(0);
        // set_gpio3(0);

        // setup tx interrupt
        JD_SET_FLAGS(JD_SERIAL_TX_DRAIN_ENABLE,21);
        timer.setCompare(MINIMUM_INTERFRAME_CC, timer.captureCounter() + (JD_MIN_INTERFRAME_SPACING + target_random(JD_SERIAL_TX_MAX_BACKOFF)));
        // DMESG("EXITE %d",test_status);
        return;
    }

    // set_gpio4(0);
    timer.setCompare(MAXIMUM_INTERBYTE_CC, timer.captureCounter() + JD_BYTE_AT_125KBAUD);
}

void JDPhysicalLayer::_timerCallback(uint16_t channels)
{
    // DMESG("TC %d",test_status);
    if (test_status & JD_SERIAL_ERR_MSK)
    {
        // DMESG("CONT ERR %d",test_status);
        JD_SET_FLAGS(JD_SERIAL_DEBUG_BIT,31);
        errorState(JDBusErrorState::Continuation);
        JD_UNSET_FLAGS(JD_SERIAL_DEBUG_BIT,31);
        return;
    }

    if (channels & (1 << MAXIMUM_INTERBYTE_CC))
    {
        if (test_status & JD_SERIAL_LO_PULSE_START)
        {
            JD_DMESG("BL ERR");
            JD_SET_FLAGS(JD_SERIAL_DEBUG_BIT,31);
            errorState(JDBusErrorState::BusLoError);
            JD_UNSET_FLAGS(JD_SERIAL_DEBUG_BIT,31);
            return;
        }
        else if (test_status & (JD_SERIAL_RECEIVING | JD_SERIAL_RECEIVING_HEADER | JD_SERIAL_TRANSMITTING))
        {
            // set_gpio3(1);
            uint32_t comparison = ((test_status & JD_SERIAL_RECEIVING_HEADER && lastBufferedCount == 0) ? JD_MAX_INTERLODATA_SPACING : JD_MAX_INTERBYTE_SPACING);
            uint32_t endTime = timer.captureCounter();
            uint32_t byteCount = (test_status & JD_SERIAL_TRANSMITTING) ? sws.getBytesTransmitted() : sws.getBytesReceived();

            // DMESG("lbc: %d br: %d",lastBufferedCount, byteCount);

            // if we've not received data since last check
            // we break up the if statements to ensure that startTime is not updated if we're waiting for bytes.
            if (lastBufferedCount == byteCount)
            {
                if (endTime - startTime >= comparison)
                {
                    JD_DMESG("BTO1");
                    JD_SET_FLAGS(JD_SERIAL_DEBUG_BIT,31);
                    errorState(JDBusErrorState::BusTimeoutError);
                    JD_UNSET_FLAGS(JD_SERIAL_DEBUG_BIT,31);
                    return;
                }
            }
            // if we have received data.
            else
            {
                startTime = endTime;
                lastBufferedCount = byteCount;
            }

            timer.setCompare(MAXIMUM_LO_DATA_CC, endTime + JD_BYTE_AT_125KBAUD);
            // set_gpio3(0);
            return;
        }
    }

    if (channels & (1 << MINIMUM_INTERFRAME_CC))
    {
        // DMESG("SEND TC");
        sendPacket();
    }
}

void JDPhysicalLayer::dmaComplete(Event evt)
{
    // DMESG("DMAC");
    if (evt.value == SWS_EVT_ERROR)
    {
        timer.clearCompare(MAXIMUM_INTERBYTE_CC);
        // we should never have the lo pulse flag set here.
        CODAL_ASSERT(!(test_status & JD_SERIAL_LO_PULSE_START), test_status);
        // DMESG("BUART ERR %d",test_status);
        JD_SET_FLAGS(JD_SERIAL_DEBUG_BIT,31);
        errorState(JDBusErrorState::BusUARTError);
        JD_UNSET_FLAGS(JD_SERIAL_DEBUG_BIT,31);
        return;
    }
    else
    {
        // rx complete, queue packet for later handling
        if (evt.value == SWS_EVT_DATA_RECEIVED)
        {
            if (test_status & JD_SERIAL_RECEIVING_HEADER)
            {
                JD_UNSET_FLAGS(JD_SERIAL_RECEIVING_HEADER,10);
                if (rxBuf->size)
                {
                    // DMESG("RXSET");
                    // here we start a new dma transaction, reset lastBufferedCount...
                    lastBufferedCount = 0;
                    sws.receiveDMA(((uint8_t*)rxBuf) + JD_SERIAL_HEADER_SIZE, rxBuf->size);
                    timer.setCompare(MAXIMUM_INTERBYTE_CC, timer.captureCounter() + JD_MAX_INTERBYTE_SPACING);
                    CODAL_ASSERT(!(test_status & JD_SERIAL_RECEIVING), test_status);
                    JD_SET_FLAGS(JD_SERIAL_RECEIVING,11);
                    return;
                }
                // DMESG("H %d %d ", rxBuf->device_address, rxBuf->size);
                // CODAL_ASSERT(rxBuf->size != 0,test_status);
                goto jd_phys_dma_exit;
            }
            else if (test_status & JD_SERIAL_RECEIVING)
            {
                JD_UNSET_FLAGS(JD_SERIAL_RECEIVING,12);
                timer.clearCompare(MAXIMUM_INTERBYTE_CC);

                // CRC is computed at the control layer.
                rxBuf->communication_rate = (uint8_t)currentBaud;
                // move rxbuf to rxArray and allocate new buffer.
                int ret = addToRxArray(rxBuf);
                if (ret == DEVICE_OK)
                {
                    DMESG("RXD[%d,%d]",this->rxHead, this->rxTail);
                    rxBuf = (JDPacket*)malloc(sizeof(JDPacket));
                    diagnostics.packets_received++;
                }
                else
                    diagnostics.packets_dropped++;

                Event(id, JD_SERIAL_EVT_DATA_READY);
            }
        }

        if (evt.value == SWS_EVT_DATA_SENT)
        {
            JD_UNSET_FLAGS(JD_SERIAL_TRANSMITTING,13);
            free(txBuf);
            txBuf = NULL;
            diagnostics.packets_sent++;
            // DMESG("DMA TXD");
        }
    }

jd_phys_dma_exit:
    sws.setMode(SingleWireDisconnected);

    // force transition to output so that the pin is reconfigured.
    // also drive the bus high for a little bit.
    sp.setDigitalValue(1);
    setState(JDSerialState::ListeningForPulse);

    JD_SET_FLAGS(JD_SERIAL_TX_DRAIN_ENABLE,22);
    timer.setCompare(MINIMUM_INTERFRAME_CC, timer.captureCounter() + (JD_MIN_INTERFRAME_SPACING + target_random(JD_SERIAL_TX_MAX_BACKOFF)));

    if (commLED)
        commLED->setDigitalValue(COMM_LED_LO);
}

void JDPhysicalLayer::loPulseDetected(uint32_t pulseTime)
{
    // JD_DMESG("LO: %d %d", (test_status & JD_SERIAL_RECEIVING) ? 1 : 0, (test_status & JD_SERIAL_TRANSMITTING) ? 1 : 0);
    // guard against repeat events.
    if (test_status & (JD_SERIAL_RECEIVING | JD_SERIAL_RECEIVING_HEADER | JD_SERIAL_TRANSMITTING) || !(test_status & DEVICE_COMPONENT_RUNNING))
        return;

    // JD_DMESG("TS: %d", pulseTime);
    pulseTime = ceil(pulseTime / 10);
    pulseTime = ceil_pow2(pulseTime);

    JD_DMESG("TS: %d", pulseTime);

    // we support 1, 2, 4, 8 as our powers of 2.
    if (pulseTime < (uint8_t)this->maxBaud || pulseTime > 8)
    {
        JD_SET_FLAGS(JD_SERIAL_DEBUG_BIT,31);
        errorState(JDBusErrorState::BusUARTError);
        JD_UNSET_FLAGS(JD_SERIAL_DEBUG_BIT,31);
        return;
    }

    // 1 us to here
    if ((JDBaudRate)pulseTime != this->currentBaud)
    {
        DMESG("SB: %d",baudToByteMap[pulseTime - 1].baud);
        sws.setBaud(baudToByteMap[pulseTime - 1].baud);
        this->currentBaud = (JDBaudRate)pulseTime;
    }
    // set_gpio(0);

    // 1 more us
    // set_gpio(1);
    // set_gpio(1);
    setState(JDSerialState::Off);

    // 10 us
    // DMESG("LO");
    JD_SET_FLAGS(JD_SERIAL_RECEIVING_HEADER,14);

    lastBufferedCount = 0;
    // set_gpio(1);
    sws.receiveDMA((uint8_t*)rxBuf, JD_SERIAL_HEADER_SIZE);
    // set_gpio(0);

    // 14 more us
    timer.setCompare(MAXIMUM_LO_DATA_CC, startTime + JD_BYTE_AT_125KBAUD);

    if (commLED)
        commLED->setDigitalValue(COMM_LED_HI);
    // DMESG("RXSTRT");
}

int JDPhysicalLayer::setState(JDSerialState state)
{
    uint32_t eventType = 0;

    if (state == JDSerialState::ListeningForPulse)
        eventType = DEVICE_PIN_INTERRUPT_ON_EDGE;

    if (state == JDSerialState::ErrorRecovery)
        eventType = DEVICE_PIN_INTERRUPT_ON_EDGE;

    if (state == JDSerialState::Off)
        eventType = DEVICE_PIN_EVENT_NONE;

    sp.eventOn(eventType);

    this->state = state;
    return sp.getDigitalValue(PullMode::Up);
}

/**
 * Constructor
 *
 * @param p the transmission pin to use
 *
 * @param sws an instance of sws created using p.
 */
JDPhysicalLayer::JDPhysicalLayer(DMASingleWireSerial&  sws, LowLevelTimer& timer, Pin* busLED, Pin* commStateLED, bool busLEDActiveLo, bool commLEDActiveLo, JDBaudRate maxBaudRate, uint16_t id) : sws(sws), sp(sws.p), timer(timer), busLED(busLED), commLED(commStateLED)
{
    this->id = id;
    this->maxBaud = maxBaudRate;
    this->sniffer = NULL;

    instance = this;

    // at least two channels are required.
    CODAL_ASSERT(timer.getChannelCount() >= TIMER_CHANNELS_REQUIRED, DEVICE_HARDWARE_CONFIGURATION_ERROR);

    rxBuf = NULL;
    txBuf = NULL;
    memset(rxArray, 0, sizeof(JDPacket*) * JD_RX_ARRAY_SIZE);
    memset(txArray, 0, sizeof(JDPacket*) * JD_TX_ARRAY_SIZE);

    this->txTail = 0;
    this->txHead = 0;

    this->rxTail = 0;
    this->rxHead = 0;

    this->busLEDActiveLo = busLEDActiveLo;
    this->commLEDActiveLo = commLEDActiveLo;

    test_status = 0;

    // 32 bit 1 us timer.
    timer.setBitMode(BitMode32);
    timer.setClockSpeed(1000);
    timer.setIRQ(jacdac_timer_irq);
    timer.enable();

    sp.setIRQ(jacdac_gpio_irq);
    sp.getDigitalValue(PullMode::None);

    sws.setDMACompletionHandler(this, &JDPhysicalLayer::dmaComplete);
}

/**
 * Retrieves the first packet on the rxQueue irregardless of the device_class
 *
 * @returns the first packet on the rxQueue or NULL
 */
JDPacket* JDPhysicalLayer::getPacket()
{
    JDPacket* pkt = popRxArray();

    if (pkt && this->sniffer)
        sniffer->handlePacket(pkt);

    return pkt;
}

/**
 * Causes this instance of JDPhysicalLayer to begin listening for packets transmitted on the serial line.
 */
void JDPhysicalLayer::start()
{
    if (isRunning())
        return;

    if (rxBuf == NULL)
        rxBuf = (JDPacket*)malloc(sizeof(JDPacket));

    JD_DMESG("JD START");

    JD_SET_FLAGS(DEVICE_COMPONENT_RUNNING,15);

    // check if the bus is lo here and change our led
    setState(JDSerialState::ListeningForPulse);

    if (busLED)
        busLED->setDigitalValue(BUS_LED_HI);

    Event(this->id, JD_SERIAL_EVT_BUS_CONNECTED);
}

/**
 * Causes this instance of JDPhysicalLayer to stop listening for packets transmitted on the serial line.
 */
void JDPhysicalLayer::stop()
{
    if (!isRunning())
        return;

    JD_UNSET_FLAGS(DEVICE_COMPONENT_RUNNING,16);
    if (rxBuf)
    {
        free(rxBuf);
        rxBuf = NULL;
    }

    setState(JDSerialState::Off);

    if (busLED)
        busLED->setDigitalValue(BUS_LED_LO);

    Event(this->id, JD_SERIAL_EVT_BUS_DISCONNECTED);
}

void JDPhysicalLayer::sendPacket()
{
    JD_DMESG("SENDP");
    // if we are receiving, the tx timer will be resumed upon reception.
    if (test_status & (JD_SERIAL_RECEIVING | JD_SERIAL_RECEIVING_HEADER | JD_SERIAL_LO_PULSE_START | JD_SERIAL_ERR_MSK))
    {
        // DMESG("RXing %d",test_status);
        return;
    }

    // if we're not transmitting and we have stuff to transmit
    if (!(test_status & JD_SERIAL_TRANSMITTING) && (this->txHead != this->txTail || txBuf))
    {
        // there is a chance for a very narrow race condition above.
        // below we disable interrupt to ensure we're not interrupted by the
        // gpio callback which operates at a higher interrupt priority.
        target_disable_irq();
        int busState = setState(JDSerialState::Off);
        target_enable_irq();

        // if the bus is lo, we shouldn't transmit (collision)
        if (busState == 0)
        {
            JD_DMESG("BUS LO");

            if (busLED)
                busLED->setDigitalValue(BUS_LED_LO);

            if (commLED)
                commLED->setDigitalValue(COMM_LED_LO);

            JD_DMESG("TXLO ERR");
            JD_SET_FLAGS(JD_SERIAL_DEBUG_BIT,31);
            errorState(JDBusErrorState::BusLoError);
            JD_UNSET_FLAGS(JD_SERIAL_DEBUG_BIT,31);
            return;
        }

        // If we get here, we assume we have control of the bus.
        // check if we have stuff to send, txBuf will be set if a previous send failed.
        JD_DMESG("TX B");
        JD_SET_FLAGS(JD_SERIAL_TRANSMITTING,17);

        // we may have a packet that we previously tried to send, but was aborted for some reason.
        if (txBuf == NULL)
            txBuf = popTxArray();

        target_disable_irq();
        sp.setDigitalValue(0);
        target_wait_us(baudToByteMap[(uint8_t)txBuf->communication_rate - 1].time_per_byte);
        sp.setDigitalValue(1);
        target_enable_irq();

        if (txBuf->communication_rate != (uint8_t)currentBaud)
        {
            sws.setBaud(baudToByteMap[(uint8_t)txBuf->communication_rate - 1].baud);
            currentBaud = (JDBaudRate)txBuf->communication_rate;
        }

        // configure to come back after the minimum lo_data gap has been observed.
        JD_SET_FLAGS(JD_SERIAL_TX_DRAIN_ENABLE,23);
        timer.setCompare(MINIMUM_INTERFRAME_CC, timer.captureCounter() + JD_MIN_INTERLODATA_SPACING);
        return;
    }

    // we've returned after a DMA transfer has been flagged (above)... start
    if (test_status & JD_SERIAL_TRANSMITTING)
    {
        // DMESG("TX S");
        startTime = timer.captureCounter();
        lastBufferedCount = 0;
        sws.sendDMA((uint8_t *)txBuf, txBuf->size + JD_SERIAL_HEADER_SIZE);
        timer.setCompare(MAXIMUM_LO_DATA_CC, startTime + JD_BYTE_AT_125KBAUD);

        if (commLED)
            commLED->setDigitalValue(COMM_LED_HI);
        return;
    }

    // if we get here, there's no more to transmit
    JD_UNSET_FLAGS(JD_SERIAL_TX_DRAIN_ENABLE,18);
    return;
}

int JDPhysicalLayer::queuePacket(JDPacket* tx)
{
    if (tx == NULL)
        return DEVICE_INVALID_PARAMETER;

    // don't queue packets if JDPhysicalLayer is not running, or the bus is being held LO.
    if (!isRunning() || test_status & JD_SERIAL_BUS_LO_ERROR)
        return DEVICE_INVALID_STATE;

    uint8_t nextTail = (this->txTail + 1) % JD_TX_ARRAY_SIZE;

    if (nextTail == this->txHead)
        return DEVICE_NO_RESOURCES;

    JDPacket* pkt = (JDPacket *)malloc(sizeof(JDPacket));
    memset(pkt, 0, sizeof(JDPacket));
    memcpy(pkt, tx, sizeof(JDPacket));

    int ret = addToTxArray(pkt);

    if (ret == DEVICE_OK && this->sniffer)
        sniffer->handlePacket(pkt);

    JD_DMESG("QU %d", pkt->size);

    if (!(test_status & JD_SERIAL_TX_DRAIN_ENABLE))
    {
        JD_DMESG("DR EN %d", timer.captureCounter());
        JD_SET_FLAGS(JD_SERIAL_TX_DRAIN_ENABLE,19);
        timer.setCompare(MINIMUM_INTERFRAME_CC, timer.captureCounter() + (JD_MIN_INTERFRAME_SPACING + target_random(JD_SERIAL_TX_MAX_BACKOFF)));
    }

    return ret;
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
int JDPhysicalLayer::send(JDPacket* tx, JDDevice* device, bool computeCRC)
{
    JD_DMESG("SEND");
    if (tx == NULL)
        return DEVICE_INVALID_PARAMETER;

    if (tx->communication_rate < (uint8_t) maxBaud)
        tx->communication_rate = (uint8_t)maxBaud;

    // crc is calculated from the address field onwards
    if (computeCRC)
    {
        uint8_t* addressPointer = (uint8_t*)&tx->device_address;
        tx->crc = jd_crc(addressPointer, tx->size + JD_SERIAL_CRC_HEADER_SIZE, device);
    }

    return queuePacket(tx);
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
int JDPhysicalLayer::send(uint8_t* buf, int len, uint8_t service_number, JDDevice* device)
{
    if (buf == NULL || len <= 0 || len > JD_SERIAL_MAX_PAYLOAD_SIZE || service_number > JD_SERIAL_MAX_SERVICE_NUMBER)
    {
        JD_DMESG("pkt TOO BIG: %d ",len);
        return DEVICE_INVALID_PARAMETER;
    }

    JDPacket pkt;
    memset(&pkt, 0, sizeof(JDPacket));

    if (device)
    {
        pkt.device_address = device->device_address;
        pkt.communication_rate = device->communication_rate;
    }
    else
    {
        pkt.device_address = 0;
        pkt.communication_rate = (uint8_t)this->getMaximumBaud();
    }

    pkt.crc = 0;
    pkt.service_number = service_number;
    pkt.size = len;

    memcpy(pkt.data, buf, len);

    return send(&pkt, device, true);
}

/**
 * Returns a bool indicating whether the JDPhysicalLayer driver has been started.
 *
 * @return true if started, false if not.
 **/
bool JDPhysicalLayer::isRunning()
{
    return (test_status & DEVICE_COMPONENT_RUNNING) ? true : false;
}

/**
 * Returns the current state if the bus.
 *
 * @return true if connected, false if there's a bad bus condition.
 **/
bool JDPhysicalLayer::isConnected()
{
    if (test_status & JD_SERIAL_ERR_MSK)
        return false;

    return true;
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
JDBusState JDPhysicalLayer::getState()
{

    if (test_status & (JD_SERIAL_RECEIVING | JD_SERIAL_RECEIVING_HEADER))
        return JDBusState::Receiving;

    if (test_status & JD_SERIAL_TRANSMITTING)
        return JDBusState::Transmitting;

    return JDBusState::Unknown;
}


uint8_t JDPhysicalLayer::getErrorState()
{
    return (test_status & JD_SERIAL_ERR_MSK) >> 4;
}

JDDiagnostics JDPhysicalLayer::getDiagnostics()
{
    return diagnostics;
}

int JDPhysicalLayer::setMaximumBaud(JDBaudRate baud)
{
    this->maxBaud = baud;
    return DEVICE_OK;
}

JDBaudRate JDPhysicalLayer::getMaximumBaud()
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
JDPacket* JDPhysicalLayer::popRxArray()
{
    // nothing to pop
    if (this->rxTail == this->rxHead)
        return NULL;

    target_disable_irq();
    uint8_t nextHead = (this->rxHead + 1) % JD_RX_ARRAY_SIZE;
    JDPacket* p = rxArray[this->rxHead];
    this->rxArray[this->rxHead] = NULL;
    this->rxHead = nextHead;
    target_enable_irq();

    // DMESG("POP[%d,%d]",this->rxHead, this->rxTail);

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
JDPacket* JDPhysicalLayer::popTxArray()
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
int JDPhysicalLayer::addToTxArray(JDPacket* packet)
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
int JDPhysicalLayer::addToRxArray(JDPacket* packet)
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
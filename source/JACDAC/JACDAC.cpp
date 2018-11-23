#include "JACDAC.h"
#include "Event.h"
#include "EventModel.h"
#include "codal_target_hal.h"
#include "CodalDmesg.h"
#include "CodalFiber.h"
#include "SingleWireSerial.h"
#include "Timer.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

#define CODAL_ASSERT(cond)                                                                         \
    if (!(cond))                                                                                   \
    target_panic(909)

using namespace codal;

#define JD_SERIAL_MAX_BACKOFF          4000
#define JD_SERIAL_MIN_BACKOFF          1000

#define JD_SERIAL_MAX_BAUD             1000000

#define JD_SERIAL_MIN_PACKET_SIZE             (JD_SERIAL_HEADER_SIZE * 10) + 5 // 5 to account for variability

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

void JACDAC::dmaComplete(Event evt)
{
    if (evt.value == SWS_EVT_ERROR)
    {
        if (status & JD_SERIAL_TRANSMITTING)
        {
            JD_DMESG("DMA TXE");
            status &= ~(JD_SERIAL_TRANSMITTING);
            free(txBuf);
            txBuf = NULL;
        }

        if (status & (JD_SERIAL_RECEIVING | JD_SERIAL_RECEIVING_HEADER))
        {
            JD_DMESG("DMA RXE");
            status &= ~(JD_SERIAL_RECEIVING | JD_SERIAL_RECEIVING_HEADER);
            system_timer_cancel_event(this->id, JD_SERIAL_EVT_RX_TIMEOUT);
            timeoutCounter = 0;
            sws.abortDMA();
            Event(this->id, JD_SERIAL_EVT_BUS_ERROR);
        }
    }
    else
    {
        // rx complete, queue packet for later handling
        if (evt.value == SWS_EVT_DATA_RECEIVED)
        {
            system_timer_cancel_event(this->id, JD_SERIAL_EVT_RX_TIMEOUT);

            if (status & JD_SERIAL_RECEIVING_HEADER)
            {

                status &= ~(JD_SERIAL_RECEIVING_HEADER);

                JDPkt* rx = (JDPkt*)rxBuf;
                DMESG("RXH %d",rx->size);
                sws.receiveDMA(((uint8_t*)rxBuf) + JD_SERIAL_HEADER_SIZE, rx->size);

                // system_timer_event_after((JD_SERIAL_MAX_BAUD / rxBaud) * (10 * rx->size), this->id, JD_SERIAL_EVT_RX_TIMEOUT);

                status |= JD_SERIAL_RECEIVING;
                return;
            }
            else if (status & JD_SERIAL_RECEIVING)
            {
                status &= ~(JD_SERIAL_RECEIVING);

                // move rxbuf to rxQueue and allocate new buffer.
                addToQueue(&rxQueue, rxBuf);
                rxBuf = (JDPkt*)malloc(sizeof(JDPkt));
                Event(id, JD_SERIAL_EVT_DATA_READY);
                DMESG("DMA RXD");
            }

        }

        if (evt.value == SWS_EVT_DATA_SENT)
        {
            status &= ~(JD_SERIAL_TRANSMITTING);
            free(txBuf);
            txBuf = NULL;
            // we've finished sending... trigger an event in random us (in some cases this might not be necessary, but it's not too much overhead).
            system_timer_event_after_us(JD_SERIAL_MIN_BACKOFF + target_random(JD_SERIAL_MAX_BACKOFF - JD_SERIAL_MIN_BACKOFF), this->id, JD_SERIAL_EVT_DRAIN);  // should be random
            JD_DMESG("DMA TXD");
        }
    }

    sws.setMode(SingleWireDisconnected);

    // force transition to output so that the pin is reconfigured.
    sp.setDigitalValue(1);
    configure(true);
}

void JACDAC::onLowPulse(Event e)
{
    // DMESG("LOW: %d %d", (status & JD_SERIAL_RECEIVING) ? 1 : 0, (status & JD_SERIAL_TRANSMITTING) ? 1 : 0);
    // guard against repeat events.
    if (status & (JD_SERIAL_RECEIVING | JD_SERIAL_RECEIVING_HEADER | JD_SERIAL_TRANSMITTING) || !(status & DEVICE_COMPONENT_RUNNING))
        return;

    uint32_t timestamp = e.timestamp / 10;
    uint32_t ceilVal = ceil_pow2((timestamp));

    DMESG("t %d c %d", timestamp, ceilVal);

    // unsupported baud rate.
    if (ceilVal == 0 || ceilVal > 8)
        return;


    if (ceilVal == 0)
        ceilVal = 1;

    rxBaud = JD_SERIAL_MAX_BAUD / ceilVal;

    sws.setBaud(rxBaud);

    sp.eventOn(DEVICE_PIN_EVENT_NONE);
    sp.getDigitalValue(PullMode::None);

    timeoutCounter = 0;
    status |= (JD_SERIAL_RECEIVING);

    sws.receiveDMA((uint8_t*)rxBuf, 32);

    // system_timer_event_after(JD_SERIAL_MAX_BAUD / rxBaud * JD_SERIAL_MIN_PACKET_SIZE, this->id, JD_SERIAL_EVT_RX_TIMEOUT);
}

void JACDAC::rxTimeout(Event)
{
    DMESG("TIMEOUT");
    sws.abortDMA();
    Event(this->id, JD_SERIAL_EVT_BUS_ERROR);
    status &= ~(JD_SERIAL_RECEIVING | JD_SERIAL_RECEIVING_HEADER);

    sws.setMode(SingleWireDisconnected);
    sp.setDigitalValue(1);
    configure(true);
}

JDPkt* JACDAC::popQueue(JDPkt** queue)
{
    if (*queue == NULL)
        return NULL;

    JDPkt *ret = *queue;

    target_disable_irq();
    *queue = (*queue)->next;
    target_enable_irq();

    return ret;
}

JDPkt* JACDAC::removeFromQueue(JDPkt** queue, uint8_t address)
{
    if (*queue == NULL)
        return NULL;

    JDPkt* ret = NULL;

    target_disable_irq();
    JDPkt *p = (*queue)->next;
    JDPkt *previous = *queue;

    if (address == (*queue)->address)
    {
        *queue = p;
        ret = previous;
    }
    else
    {
        while (p != NULL)
        {
            if (address == p->address)
            {
                ret = p;
                previous->next = p->next;
                break;
            }

            previous = p;
            p = p->next;
        }
    }

    target_enable_irq();

    return ret;
}

int JACDAC::addToQueue(JDPkt** queue, JDPkt* packet)
{
    int queueDepth = 0;
    packet->next = NULL;

    target_disable_irq();
    if (*queue == NULL)
        *queue = packet;
    else
    {
        JDPkt *p = *queue;

        while (p->next != NULL)
        {
            p = p->next;
            queueDepth++;
        }

        if (queueDepth >= JD_SERIAL_MAXIMUM_BUFFERS)
        {
            free(packet);
            return DEVICE_NO_RESOURCES;
        }

        p->next = packet;
    }
    target_enable_irq();

    return DEVICE_OK;
}

void JACDAC::configure(bool events)
{
    sp.getDigitalValue(PullMode::Up);

    if(events)
        sp.eventOn(DEVICE_PIN_EVENT_ON_PULSE);
    else
        sp.eventOn(DEVICE_PIN_EVENT_NONE);
}

/**
 * Constructor
 *
 * @param p the transmission pin to use
 *
 * @param sws an instance of sws created using p.
 */
JACDAC::JACDAC(DMASingleWireSerial&  sws, JACDACBaudRate baudRate, uint16_t id) : sws(sws), sp(sws.p)
{
    rxBuf = NULL;
    txBuf = NULL;

    rxQueue = NULL;
    txQueue = NULL;

    this->id = id;
    status = 0;

    timeoutValue = 0;
    timeoutCounter = 0;
    baud = baudRate;

    sws.setBaud(JD_SERIAL_MAX_BAUD / (uint8_t)baudRate);
    sws.setDMACompletionHandler(this, &JACDAC::dmaComplete);

    if (EventModel::defaultEventBus)
    {
        EventModel::defaultEventBus->listen(sp.id, DEVICE_PIN_EVT_PULSE_LO, this, &JACDAC::onLowPulse, MESSAGE_BUS_LISTENER_IMMEDIATE);
        EventModel::defaultEventBus->listen(this->id, JD_SERIAL_EVT_DRAIN, this, &JACDAC::sendPacket, MESSAGE_BUS_LISTENER_IMMEDIATE);
        EventModel::defaultEventBus->listen(this->id, JD_SERIAL_EVT_RX_TIMEOUT, this, &JACDAC::rxTimeout, MESSAGE_BUS_LISTENER_IMMEDIATE);
    }
}

/**
 * Retrieves the first packet on the rxQueue irregardless of the device_class
 *
 * @returns the first packet on the rxQueue or NULL
 */
JDPkt* JACDAC::getPacket()
{
    return popQueue(&rxQueue);
}

/**
 * Retrieves the first packet on the rxQueue with a matching device_class
 *
 * @param address the address filter to apply to packets in the rxQueue
 *
 * @returns the first packet on the rxQueue matching the device_class or NULL
 */
JDPkt* JACDAC::getPacket(uint8_t address)
{
    return removeFromQueue(&rxQueue, address);
}

/**
 * Causes this instance of JACDAC to begin listening for packets transmitted on the serial line.
 */
void JACDAC::start()
{
    if (isRunning())
        return;

    if (rxBuf == NULL)
        rxBuf = (JDPkt*)malloc(sizeof(JDPkt));

    JD_DMESG("JD START");

    configure(true);

    target_disable_irq();
    status = 0;
    status |= DEVICE_COMPONENT_RUNNING;
    target_enable_irq();

    Event evt(0, 0, CREATE_ONLY);

    // if the line is low, we may be in the middle of a transfer, manually trigger rx mode.
    if (sp.getDigitalValue(PullMode::Up) == 0)
    {
        JD_DMESG("TRIGGER");
        onLowPulse(evt);
    }

    sp.eventOn(DEVICE_PIN_EVENT_ON_PULSE);
}

/**
 * Causes this instance of JACDAC to stop listening for packets transmitted on the serial line.
 */
void JACDAC::stop()
{
    if (!isRunning())
        return;

    status &= ~DEVICE_COMPONENT_RUNNING;

    if (rxBuf)
    {
        free(rxBuf);
        rxBuf = NULL;
    }

    configure(false);
}

void JACDAC::sendPacket(Event)
{
    status |= JD_SERIAL_TX_DRAIN_ENABLE;
    JD_DMESG("JD SEND");
    // if we are receiving, randomly back off
    if (status & JD_SERIAL_RECEIVING || status & JD_SERIAL_RECEIVING_HEADER)
    {
        JD_DMESG("RXing");
        system_timer_event_after_us(JD_SERIAL_MIN_BACKOFF + target_random(JD_SERIAL_MAX_BACKOFF - JD_SERIAL_MIN_BACKOFF), this->id, JD_SERIAL_EVT_DRAIN);  // should be random
        return;
    }

    if (!(status & JD_SERIAL_TRANSMITTING))
    {
        // if the bus is lo, we shouldn't transmit
        if (sp.getDigitalValue(PullMode::Up) == 0)
        {
            JD_DMESG("BUS LO");
            Event evt(0, 0, CREATE_ONLY);
            onLowPulse(evt);
            system_timer_event_after_us(JD_SERIAL_MIN_BACKOFF + target_random(JD_SERIAL_MAX_BACKOFF - JD_SERIAL_MIN_BACKOFF), this->id, JD_SERIAL_EVT_DRAIN);  // should be random
            return;
        }

        // performing the above digital read will disable fall events... re-enable
        sp.setDigitalValue(1);
        configure(true);

        // if we have stuff in our queue, and we have not triggered a DMA transfer...
        if (txQueue)
        {
            JD_DMESG("TX B");
            status |= JD_SERIAL_TRANSMITTING;
            txBuf = popQueue(&txQueue);

            sp.setDigitalValue(0);
            target_wait_us(10 * (uint8_t)this->baud);
            sp.setDigitalValue(1);

            // return after 100 us
            system_timer_event_after_us(100, this->id, JD_SERIAL_EVT_DRAIN);
            return;
        }
    }

    // we've returned after a DMA transfer has been flagged (above)... start
    if (status & JD_SERIAL_TRANSMITTING)
    {
        JD_DMESG("TX S");
        sws.sendDMA((uint8_t *)txBuf, txBuf->size + JD_SERIAL_HEADER_SIZE);
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
int JACDAC::send(JDPkt* tx)
{
    if (tx == NULL)
        return DEVICE_INVALID_PARAMETER;

    if (!isRunning())
        return DEVICE_NOT_SUPPORTED;

    JDPkt* pkt = (JDPkt *)malloc(sizeof(JDPkt));
    memset(pkt, target_random(256), sizeof(JDPkt));
    memcpy(pkt, tx, sizeof(JDPkt));

    int ret = addToQueue(&txQueue, pkt);

    if (!(status & JD_SERIAL_TX_DRAIN_ENABLE))
    {
        Event e(0,0,CREATE_ONLY);
        sendPacket(e);
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
int JACDAC::send(uint8_t* buf, int len, uint8_t address)
{
    if (buf == NULL || len <= 0 || len > JD_SERIAL_DATA_SIZE)
    {
        JD_DMESG("pkt TOO BIG: %d ",len);
        return DEVICE_INVALID_PARAMETER;
    }

    JDPkt pkt;

    // for variation of crc's
    memset(&pkt, target_random(256), sizeof(JDPkt));

    // very simple crc
    pkt.crc = 0;
    pkt.address = address;
    pkt.size = len;

    memcpy(pkt.data, buf, len);

    // skip the crc.
    uint8_t* crcPointer = (uint8_t*)&pkt.address;

    // simple crc
    for (int i = 0; i < JD_SERIAL_PACKET_SIZE - 2; i++)
        pkt.crc += crcPointer[i];

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
JACDACBusState JACDAC::getState()
{
    if (status & JD_SERIAL_RECEIVING || status & JD_SERIAL_RECEIVING_HEADER)
        return JACDACBusState::Receiving;

    if (status & JD_SERIAL_TRANSMITTING)
        return JACDACBusState::Transmitting;

    // if we are neither transmitting or receiving, examine the bus.
    int busVal = sp.getDigitalValue(PullMode::Up);
    // re-enable events!
    configure(true);

    if (busVal)
        return JACDACBusState::High;

    return JACDACBusState::Low;
}

int JACDAC::setBaud(JACDACBaudRate baud)
{
    this->baud = baud;
    sws.setBaud(JD_SERIAL_MAX_BAUD / (uint8_t)baud);
}

JACDACBaudRate JACDAC::getBaud()
{
    return baud;
}
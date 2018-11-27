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

uint32_t error_count = 0;

struct BaudByte
{
    uint32_t baud;
    uint32_t time_per_byte;
};

/**
 * A simple look up for getting the time per byte given a baud rate.
 *
 * JACDACBaudRate index - 1 should be used to index this array.
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

void JACDAC::dmaComplete(Event evt)
{
    if (evt.value == SWS_EVT_ERROR)
    {
        system_timer_cancel_event(this->id, JD_SERIAL_EVT_RX_TIMEOUT);

        if (status & JD_SERIAL_TRANSMITTING)
        {
            DMESG("DMA TXE");
            status &= ~(JD_SERIAL_TRANSMITTING);
            free(txBuf);
            txBuf = NULL;
        }

        if (status & JD_SERIAL_RECEIVING)
        {
            error_count++;
            DMESG("DMA RXE");
            status &= ~(JD_SERIAL_RECEIVING);
            sws.abortDMA();
            Event(this->id, JD_SERIAL_EVT_BUS_ERROR);
        }
    }
    else
    {
        // rx complete, queue packet for later handling
        if (evt.value == SWS_EVT_DATA_RECEIVED)
        {
            status &= ~(JD_SERIAL_RECEIVING);
            system_timer_cancel_event(this->id, JD_SERIAL_EVT_RX_TIMEOUT);
            uint8_t* crcPointer = (uint8_t*)&rxBuf->address;
            uint16_t crc = fletcher16(crcPointer, JD_SERIAL_PACKET_SIZE - 2);

            if (crc == rxBuf->crc)
            {
                // move rxbuf to rxQueue and allocate new buffer.
                addToQueue(&rxQueue, rxBuf);
                rxBuf = (JDPkt*)malloc(sizeof(JDPkt));
                Event(id, JD_SERIAL_EVT_DATA_READY);
                JD_DMESG("DMA RXD");
            }
            // could we do something cool to indicate an incorrect CRC?
            // i.e. drive the bus low....?
            else
                JD_DMESG("CRCE: %d, comp: %d",rxBuf->crc, crc);
        }

        if (evt.value == SWS_EVT_DATA_SENT)
        {
            status &= ~(JD_SERIAL_TRANSMITTING);
            free(txBuf);
            txBuf = NULL;
            // we've finished sending... trigger an event in random us (in some cases this might not be necessary, but it's not too much overhead).
            system_timer_event_after_us(JD_SERIAL_TX_MIN_BACKOFF + target_random(JD_SERIAL_TX_MAX_BACKOFF - JD_SERIAL_TX_MIN_BACKOFF), this->id, JD_SERIAL_EVT_DRAIN);  // should be random
            JD_DMESG("DMA TXD");
        }
    }

    sws.setMode(SingleWireDisconnected);

    // force transition to output so that the pin is reconfigured.
    // also drive the bus high for a little bit.
    sp.setDigitalValue(1);
    configure(JACDACPinEvents::PulseEvents);
}

void JACDAC::onLowPulse(Event e)
{
    JD_DMESG("LO: %d %d", (status & JD_SERIAL_RECEIVING) ? 1 : 0, (status & JD_SERIAL_TRANSMITTING) ? 1 : 0);
    // guard against repeat events.
    if (status & (JD_SERIAL_RECEIVING | JD_SERIAL_TRANSMITTING) || !(status & DEVICE_COMPONENT_RUNNING))
        return;

    uint32_t ts = e.timestamp;
    ts = ceil(ts / 10);
    ts = ceil_pow2(ts);

    DMESG("TS: %d %d", ts, (int)e.timestamp);

    // we support 1, 2, 4, 8 as our powers of 2.
    if (ts > 8)
        return;

    // if zero round to 1 (to prevent div by 0)
    // it is assumed that the transaction is at 1 mbaud
    if (ts == 0)
        ts = 1;

    sws.setBaud(baudToByteMap[ts - 1].baud);

    sp.eventOn(DEVICE_PIN_EVENT_NONE);
    sp.getDigitalValue(PullMode::None);

    status |= (JD_SERIAL_RECEIVING);
    sws.receiveDMA((uint8_t*)rxBuf, JD_SERIAL_PACKET_SIZE);

    system_timer_event_after(baudToByteMap[ts - 1].time_per_byte * ((JD_SERIAL_PACKET_SIZE + 2)), this->id, JD_SERIAL_EVT_RX_TIMEOUT);
}

void JACDAC::rxTimeout(Event)
{
    DMESG("TIMEOUT");
    error_count++;
    sws.abortDMA();
    Event(this->id, JD_SERIAL_EVT_BUS_ERROR);
    status &= ~(JD_SERIAL_RECEIVING);
    sws.setMode(SingleWireDisconnected);
    configure(JACDACPinEvents::PulseEvents);
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

void JACDAC::configure(JACDACPinEvents eventType)
{
    sp.getDigitalValue(PullMode::Up);

    // to ensure atomicity of the state machine, we disable one event and enable the other.
    if (eventType == PulseEvents)
    {
        EventModel::defaultEventBus->listen(sp.id, DEVICE_PIN_EVT_PULSE_LO, this, &JACDAC::onLowPulse, MESSAGE_BUS_LISTENER_IMMEDIATE);
        EventModel::defaultEventBus->ignore(sp.id, DEVICE_PIN_EVT_RISE, this, &JACDAC::sendPacket);
    }

    if (eventType == EdgeEvents)
    {
        EventModel::defaultEventBus->listen(sp.id, DEVICE_PIN_EVT_RISE, this, &JACDAC::sendPacket);
        EventModel::defaultEventBus->ignore(sp.id, DEVICE_PIN_EVT_PULSE_LO, this, &JACDAC::onLowPulse);
    }

    sp.eventOn(eventType);
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

    sp.getDigitalValue(PullMode::None);

    baud = baudRate;

    sws.setBaud(baudToByteMap[(uint8_t)baudRate - 1].baud);
    sws.setDMACompletionHandler(this, &JACDAC::dmaComplete);

    if (EventModel::defaultEventBus)
    {
        EventModel::defaultEventBus->listen(this->id, JD_SERIAL_EVT_DRAIN, this, &JACDAC::sendPacket, MESSAGE_BUS_LISTENER_IMMEDIATE);
        EventModel::defaultEventBus->listen(this->id, JD_SERIAL_EVT_RX_TIMEOUT, this, &JACDAC::rxTimeout, MESSAGE_BUS_LISTENER_IMMEDIATE);
    }
}

/**
 * Retrieves the first packet on the rxQueue regardless of the device_class
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

    target_disable_irq();
    status = 0;
    status |= DEVICE_COMPONENT_RUNNING;
    target_enable_irq();

    configure(JACDACPinEvents::PulseEvents);
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

    configure(JACDACPinEvents::NoEvents);
}

void JACDAC::sendPacket(Event)
{
    status |= JD_SERIAL_TX_DRAIN_ENABLE;
    JD_DMESG("SEND");
    // if we are receiving, randomly back off
    if (status & (JD_SERIAL_RECEIVING | JD_SERIAL_BUS_RISE))
    {
        if (status & JD_SERIAL_BUS_RISE)
        {
            DMESG("RISE!!");
            status &= ~JD_SERIAL_BUS_RISE;
            configure(JACDACPinEvents::PulseEvents);
        }

        system_timer_event_after_us(JD_SERIAL_TX_MIN_BACKOFF + target_random(JD_SERIAL_TX_MAX_BACKOFF - JD_SERIAL_TX_MIN_BACKOFF), this->id, JD_SERIAL_EVT_DRAIN);
        return;
    }

    if (!(status & JD_SERIAL_TRANSMITTING))
    {
        // if the bus is lo, we shouldn't transmit
        if (sp.getDigitalValue(PullMode::Up) == 0)
        {
            DMESG("BUS LO");
            // something is holding the bus lo
            configure(JACDACPinEvents::EdgeEvents);
            // listen for when it is hi again
            status |= JD_SERIAL_BUS_RISE;
            return;
        }

        // If we get here, we assume we have control of the bus.
        // if we have stuff in our queue, and we have not triggered a DMA transfer...
        if (txQueue)
        {
            JD_DMESG("TX B");
            status |= JD_SERIAL_TRANSMITTING;
            txBuf = popQueue(&txQueue);

            sp.setDigitalValue(0);
            target_wait_us(10 * (uint8_t)baud);
            sp.setDigitalValue(1);

            // return after 100 us
            system_timer_event_after_us(100, this->id, JD_SERIAL_EVT_DRAIN);

            if (sws.getBaud() != baudToByteMap[(uint8_t)baud - 1].baud)
                sws.setBaud(baudToByteMap[(uint8_t)baud - 1].baud);
            return;
        }
    }

    // we've returned after a DMA transfer has been flagged (above)... start
    if (status & JD_SERIAL_TRANSMITTING)
    {
        JD_DMESG("TX S");
        sws.sendDMA((uint8_t *)txBuf, JD_SERIAL_PACKET_SIZE);
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

    // don't queue packets if jacdac is not running, or the bus is being held LO.
    if (!isRunning() || status & JD_SERIAL_BUS_RISE)
        return DEVICE_INVALID_STATE;

    JDPkt* pkt = (JDPkt *)malloc(sizeof(JDPkt));
    memset(pkt, 0, sizeof(JDPkt));
    memcpy(pkt, tx, sizeof(JDPkt));

    // skip the crc.
    uint8_t* crcPointer = (uint8_t*)&pkt->address;
    pkt->crc = fletcher16(crcPointer, JD_SERIAL_PACKET_SIZE - 2);

    int ret = addToQueue(&txQueue, pkt);

    if (!(status & JD_SERIAL_TX_DRAIN_ENABLE))
        system_timer_event_after_us(JD_SERIAL_TX_MIN_BACKOFF + target_random(JD_SERIAL_TX_MAX_BACKOFF - JD_SERIAL_TX_MIN_BACKOFF), this->id, JD_SERIAL_EVT_DRAIN);

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
    memset(&pkt, 0, sizeof(JDPkt));

    pkt.crc = 0;
    pkt.address = address;
    pkt.size = len;

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
    if (status & JD_SERIAL_RECEIVING)
        return JACDACBusState::Receiving;

    if (status & JD_SERIAL_TRANSMITTING)
        return JACDACBusState::Transmitting;

    // this flag is set if the bus is being held lo.
    if (status & JD_SERIAL_BUS_RISE)
        return JACDACBusState::Low;

    // if we are neither transmitting or receiving, examine the bus.
    int busVal = sp.getDigitalValue(PullMode::Up);
    // re-enable events!
    configure(JACDACPinEvents::PulseEvents);

    if (busVal)
        return JACDACBusState::High;

    return JACDACBusState::Low;
}


int JACDAC::setBaud(JACDACBaudRate baud)
{
    this->baud = baud;
    return DEVICE_OK;
}

JACDACBaudRate JACDAC::getBaud()
{
    return baud;
}
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

void JACDAC::dmaComplete(Event evt)
{
    JD_DMESG("DMA");
    if (evt.value == SWS_EVT_ERROR)
    {
        JD_DMESG("ERR");
        if (status & JD_SERIAL_TRANSMITTING)
        {
            JD_DMESG("TX ERROR");
            status &= ~(JD_SERIAL_TRANSMITTING);
            free(txBuf);
            txBuf = NULL;
        }

        if (status & JD_SERIAL_RECEIVING)
        {
            JD_DMESG("RX ERROR");
            status &= ~(JD_SERIAL_RECEIVING);
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
            status &= ~(JD_SERIAL_RECEIVING);
            // move rxbuf to rxQueue and allocate new buffer.
            addToQueue(&rxQueue, rxBuf);
            rxBuf = (JDPkt*)malloc(sizeof(JDPkt));
            Event(id, JD_SERIAL_EVT_DATA_READY);
        }

        if (evt.value == SWS_EVT_DATA_SENT)
        {
            status &= ~(JD_SERIAL_TRANSMITTING);
            free(txBuf);
            txBuf = NULL;
            // we've finished sending... trigger an event in random us (in some cases this might not be necessary, but it's not too much overhead).
            system_timer_event_after_us(4000, this->id, JD_SERIAL_EVT_DRAIN);  // should be random
            JD_DMESG("TX DONE");
        }
    }

    sws.setMode(SingleWireDisconnected);

    // force transition to output so that the pin is reconfigured.
    sp.setDigitalValue(1);
    configure(true);
}

void JACDAC::onFallingEdge(Event)
{
    // JD_DMESG("FALL: %d %d", (status & JD_SERIAL_RECEIVING) ? 1 : 0, (status & JD_SERIAL_TRANSMITTING) ? 1 : 0);
    // guard against repeat events.
    if (status & (JD_SERIAL_RECEIVING | JD_SERIAL_TRANSMITTING) || !(status & DEVICE_COMPONENT_RUNNING))
        return;

    sp.eventOn(DEVICE_PIN_EVENT_NONE);
    sp.getDigitalValue(PullMode::None);

    timeoutCounter = 0;
    status |= (JD_SERIAL_RECEIVING);

    sws.receiveDMA((uint8_t*)rxBuf, JD_SERIAL_PACKET_SIZE);
}

void JACDAC::periodicCallback()
{
    // calculate 1 packet at baud
    if (timeoutCounter == 0)
    {
        uint32_t timePerSymbol = 1000000/sws.getBaud();
        timePerSymbol = timePerSymbol * 100 * JD_SERIAL_PACKET_SIZE;
        timeoutValue = (timePerSymbol / SCHEDULER_TICK_PERIOD_US) + 2;
    }

    if (status & JD_SERIAL_RECEIVING)
    {
        JD_DMESG("H");
        timeoutCounter++;

        if (timeoutCounter > timeoutValue)
        {
            JD_DMESG("TIMEOUT");
            sws.abortDMA();
            Event(this->id, JD_SERIAL_EVT_BUS_ERROR);
            timeoutCounter = 0;
            status &= ~(JD_SERIAL_RECEIVING);

            sws.setMode(SingleWireDisconnected);
            sp.setDigitalValue(1);
            configure(true);
        }
    }
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
        sp.eventOn(DEVICE_PIN_EVENT_ON_EDGE);
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
JACDAC::JACDAC(codal::Pin& p, DMASingleWireSerial&  sws, uint16_t id) : sws(sws), sp(p)
{
    rxBuf = NULL;
    txBuf = NULL;

    rxQueue = NULL;
    txQueue = NULL;

    this->id = id;
    status = 0;

    timeoutValue = 0;
    timeoutCounter = 0;

    sws.setBaud(1000000);
    sws.setDMACompletionHandler(this, &JACDAC::dmaComplete);

    if (EventModel::defaultEventBus)
    {
        EventModel::defaultEventBus->listen(sp.id, DEVICE_PIN_EVT_FALL, this, &JACDAC::onFallingEdge, MESSAGE_BUS_LISTENER_IMMEDIATE);
        EventModel::defaultEventBus->listen(this->id, JD_SERIAL_EVT_DRAIN, this, &JACDAC::sendPacket, MESSAGE_BUS_LISTENER_IMMEDIATE);
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
    if (rxBuf == NULL)
        rxBuf = (JDPkt*)malloc(sizeof(JDPkt));

    JD_DMESG("JD START");

    configure(true);

    target_disable_irq();
    status = 0;
    status |= (DEVICE_COMPONENT_RUNNING | DEVICE_COMPONENT_STATUS_SYSTEM_TICK);
    target_enable_irq();

    Event evt(0, 0, CREATE_ONLY);

    // if the line is low, we may be in the middle of a transfer, manually trigger rx mode.
    if (sp.getDigitalValue(PullMode::Up) == 0)
    {
        JD_DMESG("TRIGGER");
        onFallingEdge(evt);
    }

    sp.eventOn(DEVICE_PIN_EVENT_ON_EDGE);
}

/**
 * Causes this instance of JACDAC to stop listening for packets transmitted on the serial line.
 */
void JACDAC::stop()
{
    status &= ~(DEVICE_COMPONENT_RUNNING | DEVICE_COMPONENT_STATUS_SYSTEM_TICK);
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
    if (status & JD_SERIAL_RECEIVING)
    {
        JD_DMESG("RXing");
        system_timer_event_after_us(4000, this->id, JD_SERIAL_EVT_DRAIN);  // should be random
        return;
    }

    if (!(status & JD_SERIAL_TRANSMITTING))
    {
        // if the bus is lo, we shouldn't transmit
        if (sp.getDigitalValue(PullMode::Up) == 0)
        {
            JD_DMESG("BUS LO");
            Event evt(0, 0, CREATE_ONLY);
            onFallingEdge(evt);
            system_timer_event_after_us(4000, this->id, JD_SERIAL_EVT_DRAIN);  // should be random
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
            target_wait_us(10);
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

bool JACDAC::isRunning()
{
    return (status & DEVICE_COMPONENT_RUNNING) ? true : false;
}
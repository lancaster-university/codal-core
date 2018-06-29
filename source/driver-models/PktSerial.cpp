#include "PktSerial.h"
#include "Event.h"
#include "EventModel.h"
#include "codal_target_hal.h"
#include "CodalDmesg.h"
#include "CodalFiber.h"
#include "SingleWireSerial.h"
#include "Timer.h"

extern "C" void wait_us(uint32_t);

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

#define CODAL_ASSERT(cond)                                                                         \
    if (!(cond))                                                                                   \
    target_panic(909)

using namespace codal;


void PktSerial::dmaComplete(Event evt)
{
    codal_dmesg("DMA");
    if (evt.value == SWS_EVT_ERROR)
    {
        codal_dmesg("ERR");
        if (status & PKT_SERIAL_TRANSMITTING)
        {
            codal_dmesg("TX ERROR");
            // if the packet is flagged as lossy, we move on.
            if (txBuf->flags & PKT_PKT_FLAGS_LOSSY)
            {
                status &= ~(PKT_SERIAL_TRANSMITTING);
                free(txBuf);
                txBuf = NULL;
            }
            else
                // don't unset the flag, send the same packet again.
                system_timer_event_after_us(4000, this->id, PKT_SERIAL_EVT_DRAIN);  // should be random
        }
    }
    else
    {
        // rx complete, queue packet for later handling
        if (evt.value == SWS_EVT_DATA_RECEIVED)
        {
            status &= ~(PKT_SERIAL_RECEIVING);
            codal_dmesg("RX END");
            // move rxbuf to rxQueue and allocate new buffer.
            addToQueue(&rxQueue, rxBuf);
            rxBuf = (PktSerialPkt*)malloc(sizeof(PktSerialPkt));
            Event(id, PKT_SERIAL_EVT_DATA_READY);
        }

        if (evt.value == SWS_EVT_DATA_SENT)
        {
            status &= ~(PKT_SERIAL_TRANSMITTING);
            codal_dmesg("TX END");
            free(txBuf);
            txBuf = NULL;
            // we've finished sending... trigger an event in random us (in some cases this might not be necessary, but it's not too much overhead).
            system_timer_event_after_us(4000, this->id, PKT_SERIAL_EVT_DRAIN);  // should be random
        }
    }

    sws.setMode(SingleWireDisconnected);

    // force transition to output so that the pin is reconfigured.
    sp.setDigitalValue(1);
    configure(true);
}

void PktSerial::onFallingEdge(Event)
{
    codal_dmesg("FALL: %d %d", (status & PKT_SERIAL_RECEIVING) ? 1 : 0, (status & PKT_SERIAL_TRANSMITTING) ? 1 : 0);
    // guard against repeat events.
    if (status & (PKT_SERIAL_RECEIVING | PKT_SERIAL_TRANSMITTING) || !(status & DEVICE_COMPONENT_RUNNING))
        return;

    sp.eventOn(DEVICE_PIN_EVENT_NONE);
    sp.getDigitalValue(PullMode::None);

    timeoutCounter = 0;
    status |= (PKT_SERIAL_RECEIVING);

    codal_dmesg("RX START");
    sws.receiveDMA((uint8_t*)rxBuf, PKT_SERIAL_PACKET_SIZE);
}

void PktSerial::onRisingEdge(Event)
{

}

PktSerial::PktSerial(codal::Pin& p, DMASingleWireSerial&  sws, uint16_t id) : sws(sws), sp(p)
{
    rxBuf = NULL;
    txBuf = NULL;

    rxQueue = NULL;
    txQueue = NULL;

    this->id = id;
    status = 0;

    sws.setBaud(1000000);
    sws.setDMACompletionHandler(this, &PktSerial::dmaComplete);

    if (EventModel::defaultEventBus)
    {
        EventModel::defaultEventBus->listen(sp.id, DEVICE_PIN_EVT_FALL, this, &PktSerial::onFallingEdge, MESSAGE_BUS_LISTENER_IMMEDIATE);
        // EventModel::defaultEventBus->listen(sp.id, DEVICE_PIN_EVT_RISE, this, &PktSerial::onRisingEdge, MESSAGE_BUS_LISTENER_IMMEDIATE);
        EventModel::defaultEventBus->listen(this->id, PKT_SERIAL_EVT_DRAIN, this, &PktSerial::sendPacket, MESSAGE_BUS_LISTENER_IMMEDIATE);
    }
}

void PktSerial::periodicCallback()
{
    if (status & PKT_SERIAL_RECEIVING)
    {
        codal_dmesg("H");
        if (timeoutCounter >= PKT_SERIAL_DMA_TIMEOUT)
        {
            codal_dmesg("ABORT");
            sws.abortDMA();
            sws.setMode(SingleWireDisconnected);
            sp.setDigitalValue(1);
            configure(true);

            Event(this->id, PKT_SERIAL_EVT_BUS_ERROR);
            timeoutCounter = 0;
            status &=~(PKT_SERIAL_RECEIVING);
        }
        else
            timeoutCounter++;
    }
}

PktSerialPkt* PktSerial::popQueue(PktSerialPkt** queue)
{
    if (*queue == NULL)
        return NULL;

    PktSerialPkt *ret = *queue;

    target_disable_irq();
    *queue = (*queue)->next;
    target_enable_irq();

    return ret;
}

PktSerialPkt* PktSerial::removeFromQueue(PktSerialPkt** queue, uint8_t device_class)
{
    if (*queue == NULL)
        return NULL;

    PktSerialPkt* ret = NULL;

    target_disable_irq();
    PktSerialPkt *p = (*queue)->next;
    PktSerialPkt *previous = *queue;

    if (device_class == (*queue)->device_class)
    {
        *queue = p;
        ret = previous;
    }
    else
    {
        while (p != NULL)
        {
            if (device_class == p->device_class)
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

int PktSerial::addToQueue(PktSerialPkt** queue, PktSerialPkt* packet)
{
    int queueDepth = 0;
    packet->next = NULL;

    target_disable_irq();
    if (*queue == NULL)
        *queue = packet;
    else
    {
        PktSerialPkt *p = *queue;

        while (p->next != NULL)
        {
            p = p->next;
            queueDepth++;
        }

        if (queueDepth >= PKT_SERIAL_MAXIMUM_BUFFERS)
        {
            free(packet);
            return DEVICE_NO_RESOURCES;
        }

        p->next = packet;
    }
    target_enable_irq();

    return DEVICE_OK;
}

void PktSerial::configure(bool events)
{
    sp.getDigitalValue(PullMode::Up);

    codal_dmesg("%p", *(volatile uint32_t *) 0x4002000c);

    if(events)
        sp.eventOn(DEVICE_PIN_EVENT_ON_EDGE);
    else
        sp.eventOn(DEVICE_PIN_EVENT_NONE);

    codal_dmesg("%p", *(volatile uint32_t *) 0x4002000c);
}

PktSerialPkt* PktSerial::getPacket()
{
    return popQueue(&rxQueue);
}

/**
* Start to listen.
*/
void PktSerial::start()
{
    if (rxBuf == NULL)
        rxBuf = (PktSerialPkt*)malloc(sizeof(PktSerialPkt));

    configure(true);

    target_disable_irq();
    status = 0;
    status |= (DEVICE_COMPONENT_RUNNING | DEVICE_COMPONENT_STATUS_SYSTEM_TICK);
    target_enable_irq();

    Event evt(0, 0, CREATE_ONLY);

    // if the line is low, we may be in the middle of a transfer, manually trigger rx mode.
    if (sp.getDigitalValue(PullMode::Up) == 0)
    {
        codal_dmesg("TRIGGER");
        onFallingEdge(evt);
    }

    sp.eventOn(DEVICE_PIN_EVENT_ON_EDGE);
}

/**
* Disables protocol.
*/
void PktSerial::stop()
{
    status &= ~(DEVICE_COMPONENT_RUNNING | DEVICE_COMPONENT_STATUS_SYSTEM_TICK);
    if (rxBuf)
    {
        free(rxBuf);
        rxBuf = NULL;
    }

    configure(false);
}

/**
* Writes to the PktSerial bus. Waits (possibly un-scheduled) for transfer to finish.
*/
extern void wait_us(uint32_t);
void PktSerial::sendPacket(Event)
{
    // codal_dmesg("DRAIN");
    status |= PKT_SERIAL_TX_DRAIN_ENABLE;

    // if we are receiving, randomly back off
    if (status & PKT_SERIAL_RECEIVING)
    {
        codal_dmesg("RXing");
        system_timer_event_after_us(4000, this->id, PKT_SERIAL_EVT_DRAIN);  // should be random
        return;
    }

    if (!(status & PKT_SERIAL_TRANSMITTING))
    {
        // if the bus is lo, we shouldn't transmit
        if (sp.getDigitalValue(PullMode::Up) == 0)
        {
            codal_dmesg("BUS LO");
            Event evt(0, 0, CREATE_ONLY);
            onFallingEdge(evt);
            system_timer_event_after_us(4000, this->id, PKT_SERIAL_EVT_DRAIN);  // should be random
            return;
        }

        // performing the above digital read will disable fall events... re-enable
        sp.setDigitalValue(1);
        configure(true);

        // if we have stuff in our queue, and we have not triggered a DMA transfer...
        if (txQueue)
        {
            codal_dmesg("TX B");
            status |= PKT_SERIAL_TRANSMITTING;
            txBuf = popQueue(&txQueue);

            sp.setDigitalValue(0);

            // return after 100 us
            system_timer_event_after_us(100, this->id, PKT_SERIAL_EVT_DRAIN);
            return;
        }

        // sp.eventOn(DEVICE_PIN_EVENT_ON_EDGE);
    }

    // we've returned after a DMA transfer has been flagged (above)... start
    if (status & PKT_SERIAL_TRANSMITTING)
    {
        codal_dmesg("TX S");
        sp.setDigitalValue(1);
        sws.sendDMA((uint8_t *)txBuf, PKT_SERIAL_PACKET_SIZE);
        return;
    }

    // codal_dmesg("D_DISABLE");
    // if we get here, there's no more to transmit
    status &= ~(PKT_SERIAL_TX_DRAIN_ENABLE);
    return;
}

int PktSerial::send(PktSerialPkt *pkt)
{
    int ret = addToQueue(&txQueue, pkt);

    if (!(status & PKT_SERIAL_TX_DRAIN_ENABLE))
    {
        Event e(0,0,CREATE_ONLY);
        sendPacket(e);
    }

    return ret;
}

int PktSerial::send(uint8_t* buf, int len)
{
    if (buf == NULL || len <= 0 || len >= PKT_SERIAL_DATA_SIZE)
        return DEVICE_INVALID_PARAMETER;

    PktSerialPkt* pkt = (PktSerialPkt*)malloc(sizeof(PktSerialPkt));
    memset(pkt, 0, sizeof(PktSerialPkt));

    // very simple crc
    pkt->crc = 0;
    pkt->size = len;

    memcpy(pkt->data, buf, len);

    // skip the crc.
    uint8_t* crcPointer = (uint8_t*)&pkt->size;

    // simple crc
    for (int i = 0; i < PKT_SERIAL_PACKET_SIZE - 2; i++)
        pkt->crc += crcPointer[i];

    return send(pkt);
}
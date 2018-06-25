#include "PktSerial.h"
#include "Event.h"
#include "EventModel.h"
#include "codal_target_hal.h"
#include "CodalFiber.h"
#include "SingleWireSerial.h"

#define CODAL_ASSERT(cond)                                                                         \
    if (!(cond))                                                                                   \
    target_panic(909)

using namespace codal;

PktSerialPkt *PktSerialPkt::allocate(PktSerialPkt& p)
{
    PktSerialPkt *newPacket = (PktSerialPkt *)malloc(PKT_SERIAL_HEADER_SIZE + p.size);
    memset(newPacket, 0, PKT_SERIAL_HEADER_SIZE + p.size);
    memcpy(newPacket, &p, PKT_SERIAL_HEADER_SIZE);
    return newPacket;
}

PktSerialPkt *PktSerialPkt::allocate(uint16_t size)
{
    PktSerialPkt *newPacket = (PktSerialPkt *)malloc(PKT_SERIAL_HEADER_SIZE + size);
    memset(newPacket, 0, PKT_SERIAL_HEADER_SIZE + size);
    newPacket->size = size;
    return newPacket;
}

void PktSerial::dmaComplete(Event evt)
{
    // rx complete, queue packet for later handling
    if (evt.value == SWS_EVT_DATA_RECEIVED)
    {
        status &= ~PKT_SERIAL_RECEIVING;
        queuePacket();
    }

    if (evt.value == SWS_EVT_DATA_SENT)
        status &= ~PKT_SERIAL_TRANSMITTING;

    // release any waiting fibers.
    evt.source = this->id;
    evt.fire();
}

void PktSerial::onRisingEdge(Event)
{
    status |= PKT_SERIAL_RECEIVING;

    PktSerialPkt p;
    // spin receive our header
    int ret = sws.receive((uint8_t*)&p, PKT_SERIAL_HEADER_SIZE);

    // abort
    if (ret == DEVICE_CANCELLED)
        return;

    rxBuf = PktSerialPkt::allocate(p);
    sws.receiveDMA((uint8_t*)rxBuf + PKT_SERIAL_HEADER_SIZE, rxBuf->size);
}

PktSerial::PktSerial(codal::Pin& p, DMASingleWireSerial&  sws, uint16_t id) : sws(sws), sp(p)
{
    rxBuf = NULL;
    this->id = id;

    memset(packetBuffer, 0, sizeof(PktSerialPkt) * PKT_SERIAL_MAX_BUFFERS);
    bufferTail = 0;

    sws.setBaud(115200);
    sws.setDMACompletionHandler(this, &PktSerial::dmaComplete);

    if (EventModel::defaultEventBus)
        EventModel::defaultEventBus->listen(sp.id, DEVICE_PIN_EVT_RISE, this, &PktSerial::onRisingEdge, MESSAGE_BUS_LISTENER_IMMEDIATE);
}

PktSerialPkt* PktSerial::getPacket()
{
    PktSerialPkt* p = packetBuffer[bufferTail];
    bufferTail = (bufferTail + 1) % PKT_SERIAL_MAX_BUFFERS;
    return p;
}

int PktSerial::queuePacket()
{
    if (rxBuf == NULL)
        return DEVICE_INVALID_PARAMETER;

    int i = 0;

    for (; i < PKT_SERIAL_MAX_BUFFERS; i++)
    {
        if (packetBuffer[i] == NULL)
        {
            packetBuffer[i] = rxBuf;
            Event(this->id, PKT_SERIAL_DATA_READY);
            break;
        }
    }

    // null-ify for our next buffer.
    rxBuf = NULL;
    return DEVICE_OK;
}

/**
* Start to listen.
*/
void PktSerial::start()
{
    sp.setPull(PullMode::Down);
    sp.getDigitalValue();
    sp.eventOn(DEVICE_PIN_EVENT_ON_EDGE);
}

/**
* Disables protocol.
*/
void PktSerial::stop()
{
    sp.setPull(PullMode::Down);
    sp.getDigitalValue();
    sp.eventOn(DEVICE_PIN_EVENT_NONE);
}

/**
* Writes to the PktSerial bus. Waits (possibly un-scheduled) for transfer to finish.
*/
extern "C" void wait_us(uint32_t);
int PktSerial::send(const PktSerialPkt *pkt)
{
    // in time this should block until free
    if (sp.getDigitalValue() != 0 || status & PKT_SERIAL_RECEIVING)
        return DEVICE_CANCELLED;

    sp.setDigitalValue(1);

    // this needs to be a target function...
    wait_us(100);
    sp.setPull(PullMode::None);
    sp.getDigitalValue();
    int ret = sws.send((uint8_t *)pkt, PKT_SERIAL_HEADER_SIZE);

    // this needs to be a target function...
    wait_us(30);
    ret = sws.sendDMA((uint8_t *)pkt + PKT_SERIAL_HEADER_SIZE, pkt->size);

    if (ret != DEVICE_OK)
        return DEVICE_CANCELLED;

    fiber_wake_on_event(this->id, SWS_EVT_DATA_SENT);
    schedule();

    sws.setMode(SingleWireDisconnected);
    sp.setPull(PullMode::Down);
    sp.getDigitalValue();

    return DEVICE_OK;
}

int PktSerial::send(uint8_t* buf, int len)
{
    if (buf == NULL || len <= 0)
        return DEVICE_INVALID_PARAMETER;

    PktSerialPkt* pkt = PktSerialPkt::allocate(len);

    // very simple crc
    pkt->crc = 0;

    for (int i = 0; i < len; i++)
        pkt->crc += buf[i];

    memcpy((uint8_t*)pkt + PKT_SERIAL_HEADER_SIZE, buf, len);

    return send(pkt);
}
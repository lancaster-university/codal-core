#include "PktSerial.h"
#include "Event.h"
#include "EventModel.h"
#include "codal_target_hal.h"

using namespace codal;

void PktSerial::onRisingEdge(Event)
{
    sws.setMode(SingleWireRx);
}

PktSerial::PktSerial(codal::Pin& p, SingleWireSerial&  sws) : sws(sws), sp(p)
{
    if (EventModel::defaultEventBus)
        EventModel::defaultEventBus->listen(sp.id, DEVICE_PIN_EVT_RISE, this, &PktSerial::onRisingEdge, MESSAGE_BUS_LISTENER_IMMEDIATE);
}

PktSerialPkt* PktSerial::getPacket()
{
    // return sws.getPacket();
    return NULL;
}

/**
* Start to listen.
*/
void PktSerial::start()
{
    sp.setPull(PullMode::Down);
    sp.getDigitalValue();
    sp.eventOn(DEVICE_PIN_EVENT_ON_EDGE);
    sws.setBaud(0);
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
int PktSerial::send(const PktSerialPkt *pkt)
{
    uint8_t *p = (uint8_t *)pkt;

    sp.setDigitalValue(1);
    target_wait(100);
    sws.setMode(SingleWireTx);

    while(p < p + PCKT_SIZE_SIZE)
        sws.putc(*p++);

    sws.sendBreak();

    while(p < p + (pkt->size - PCKT_SIZE_SIZE))
        sws.putc(*p++);

    sws.sendBreak();

    sws.setMode(SingleWireRx);
}
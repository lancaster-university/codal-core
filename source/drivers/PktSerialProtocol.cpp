/*
The MIT License (MIT)

Copyright (c) 2017 Lancaster University.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#include "CodalConfig.h"
#include "ErrorNo.h"
#include "Event.h"
#include "EventModel.h"
#include "PktSerialProtocol.h"
#include "Timer.h"

using namespace codal;

void PktSerialProtocol::sendControl(Event)
{

}

void PktSerialProtocol::periodicCallback()
{

}

void PktSerialProtocol::onPacketReceived(Event)
{
    PktSerialPkt* pkt = bus.getPacket();

    // drop for now
    if (pkt)
        delete pkt;
}

PktSerialProtocol::PktSerialProtocol(PktSerial& pkt, uint32_t serialNumber, uint16_t id) : bus(pkt)
{
    this->id = id;

    if (EventModel::defaultEventBus)
    {
        EventModel::defaultEventBus->listen(bus.id, PKT_SERIAL_EVT_DATA_READY, this, &PktSerialProtocol::onPacketReceived, MESSAGE_BUS_LISTENER_IMMEDIATE);
        EventModel::defaultEventBus->listen(this->id, PKT_PROTOCOL_EVT_SEND_CONTROL, this, &PktSerialProtocol::sendControl, MESSAGE_BUS_LISTENER_IMMEDIATE);
    }
}

int PktSerialProtocol::add(PktSerialDriver& device)
{
    device;
    return DEVICE_OK;
}

int PktSerialProtocol::remove(PktSerialDriver& device)
{
    device;
    return DEVICE_OK;
}

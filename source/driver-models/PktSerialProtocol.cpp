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

PktSerialDriver* PktSerialProtocol::drivers[PKT_PROTOCOL_DRIVER_SIZE] = { 0 };

void PktSerialProtocol::onPacketReceived(Event)
{
    PktSerialPkt* pkt = bus.getPacket();

    // if this packet is destined for our drivers...
    if (!logic.filterPacket(pkt->address))
    {
        for (int i = 0; i < PKT_PROTOCOL_DRIVER_SIZE; i++)
        {
            if (this->drivers[i] && this->drivers[i]->device.address == pkt->address && this->drivers[i]->device.flags & PKT_DEVICE_FLAGS_INITIALISED)
            {
                this->drivers[i]->handlePacket(pkt);
                break;
            }
        }
    }

    free(pkt);
}

PktSerialProtocol::PktSerialProtocol(PktSerial& pkt, uint16_t id) : logic(*this), bus(pkt)
{
    this->id = id;

    add(logic);

    memset(this->drivers, 0, sizeof(PktSerialDriver*) * PKT_PROTOCOL_DRIVER_SIZE);

    if (EventModel::defaultEventBus)
        EventModel::defaultEventBus->listen(bus.id, PKT_SERIAL_EVT_DATA_READY, this, &PktSerialProtocol::onPacketReceived, MESSAGE_BUS_LISTENER_IMMEDIATE);
}

int PktSerialProtocol::add(PktSerialDriver& driver)
{
    int i;

    for (i = 0; i < PKT_PROTOCOL_DRIVER_SIZE; i++)
    {
        if (drivers[i] == NULL)
        {
            drivers[i] = &driver;
            break;
        }
    }

    if (i == PKT_PROTOCOL_DRIVER_SIZE)
        return DEVICE_NO_RESOURCES;

    return DEVICE_OK;
}

int PktSerialProtocol::remove(PktSerialDriver& driver)
{
    for (int i = 0; i < PKT_PROTOCOL_DRIVER_SIZE; i++)
    {
        if (drivers[i] == &driver)
        {
            drivers[i] = NULL;
            break;
        }
    }

    return DEVICE_OK;
}

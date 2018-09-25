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
#include "JDProtocol.h"
#include "Timer.h"
#include "CodalDmesg.h"

using namespace codal;

JDDriver* JDProtocol::drivers[JD_PROTOCOL_DRIVER_ARRAY_SIZE] = { 0 };

JDProtocol* JDProtocol::instance = NULL;

void JDProtocol::onPacketReceived(Event)
{
    JDPkt* pkt = NULL;

    while((pkt = bus.getPacket()) != NULL)
    {
        JD_DMESG("pkt REC ADDR: %d",pkt->address);

        // if this packet is destined for our drivers...
        // address 0 will never be filtered.
        if (!logic.filterPacket(pkt->address))
        {
            uint32_t driver_class = 0;

            JD_DMESG("NOT FILTERED");
            for (int i = 0; i < JD_PROTOCOL_DRIVER_ARRAY_SIZE; i++)
            {
                if (this->drivers[i])
                {
                    // could be optimised into a single if, but useful for debugging.
                    JD_DMESG("DRIV a:%d sn:%d i:%d", this->drivers[i]->device.address, this->drivers[i]->device.serial_number, this->drivers[i]->device.flags & JD_DEVICE_FLAGS_INITIALISED ? 1 : 0);
                    if ((this->drivers[i]->device.flags & JD_DEVICE_FLAGS_INITIALISED) && this->drivers[i]->device.address == pkt->address)
                    {
                        if (this->drivers[i]->device.flags & JD_DEVICE_FLAGS_BROADCAST_MAP)
                        {
                            JD_DMESG("BROADMAP DETECTED");
                            driver_class = this->drivers[i]->device.driver_class;
                        }
                        else
                        {
                            // DMESG("HANDLED BY LOCAL / REMOTE A: %d", this->drivers[i]->getAddress());
                            this->drivers[i]->handlePacket(pkt);
                        }

                        break; // only one address per device, lets break early
                    }
                }
            }

            // if we've matched a broadcast map, it means we need to map a broadcast packet to any driver of the same class
            if (driver_class > 0)
                for (int i = 0; i < JD_PROTOCOL_DRIVER_ARRAY_SIZE; i++)
                {
                    if ((this->drivers[i]->device.flags & JD_DEVICE_FLAGS_BROADCAST) && this->drivers[i]->device.driver_class == driver_class)
                    {
                        JD_DMESG("HANDLED BY BROADCAST");
                        this->drivers[i]->handlePacket(pkt);
                    }
                }
        }

        if (bridge != NULL)
            bridge->handlePacket(pkt);

        free(pkt);
    }
}

JDProtocol::JDProtocol(JACDAC& jacdac, uint16_t id) : logic(), bridge(NULL), bus(jacdac)
{
    this->id = id;

    if (instance == NULL)
        instance = this;

    add(logic);

    memset(this->drivers, 0, sizeof(JDDriver*) * JD_PROTOCOL_DRIVER_ARRAY_SIZE);

    // packets are queued, and should be processed in normal context.
    if (EventModel::defaultEventBus)
        EventModel::defaultEventBus->listen(jacdac.id, JD_SERIAL_EVT_DATA_READY, this, &JDProtocol::onPacketReceived);
}

int JDProtocol::add(JDDriver& driver)
{
    int i;

    // check for duplicates first
    for (i = 0; i < JD_PROTOCOL_DRIVER_ARRAY_SIZE; i++)
        if (drivers[i] == &driver)
            return DEVICE_OK;

    for (i = 0; i < JD_PROTOCOL_DRIVER_ARRAY_SIZE; i++)
    {
        target_disable_irq();
        if (drivers[i] == NULL)
        {
            drivers[i] = &driver;
            break;
        }
        target_enable_irq();
    }

    if (i == JD_PROTOCOL_DRIVER_ARRAY_SIZE)
        return DEVICE_NO_RESOURCES;

    return DEVICE_OK;
}

int JDProtocol::remove(JDDriver& driver)
{
    target_disable_irq();
    for (int i = 0; i < JD_PROTOCOL_DRIVER_ARRAY_SIZE; i++)
    {
        if (drivers[i] == &driver)
        {
            drivers[i] = NULL;
            break;
        }
    }
    target_enable_irq();

    return DEVICE_OK;
}

int JDProtocol::setBridge(JDDriver& bridge)
{
    this->bridge = &bridge;
    return DEVICE_OK;
}

int JDProtocol::send(JDPkt* pkt)
{
    if (instance)
        return instance->bus.send(pkt);

    return DEVICE_NO_RESOURCES;
}

int JDProtocol::send(uint8_t* buf, int len, uint8_t address)
{
    if (instance)
        return instance->bus.send(buf, len, address);

    return DEVICE_NO_RESOURCES;
}

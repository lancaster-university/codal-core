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

static char jacdac_name[JD_CONTROL_PACKET_ERROR_NAME_LENGTH] = {'J', 'A', 'C', 'D', 'A', 'C'};

using namespace codal;

JDDriver* JDProtocol::drivers[JD_PROTOCOL_DRIVER_ARRAY_SIZE] = { 0 };

JDProtocol* JDProtocol::instance = NULL;

void JDProtocol::onPacketReceived(Event)
{
    JDPacket* pkt = NULL;

    while((pkt = bus.getPacket()) != NULL)
    {
        JD_DMESG("pkt REC AD: %d SZ:%d",pkt->address, pkt->size);

        uint32_t driver_class = 0;

        // check if this packet is destined for our drivers...
        // address 0 will never be filtered.
        if (!logic.filterPacket(pkt->address))
        {
            JD_DMESG("NOT FILTERED");
            for (int i = 0; i < JD_PROTOCOL_DRIVER_ARRAY_SIZE; i++)
            {
                JDDriver* driver = this->drivers[i];

                if (driver)
                {
                    // the above could be optimised into a single if, but useful for debugging.
                    JD_DMESG("DRIV a:%d sn:%d c:%d i:%d f %d", driver->device.address, driver->device.serial_number, driver->device.driver_class, driver->device.flags & JD_DEVICE_FLAGS_INITIALISED ? 1 : 0, driver->device.flags);

                    // if the address is the same, or we're matching on class...
                    if ((driver->device.flags & JD_DEVICE_FLAGS_INITIALISED) && driver->device.address == pkt->address)
                    {
                        driver_class = driver->device.driver_class;

                        // break if the device is a broadcast to prevent duplicate receptions
                        // break if DEVICE_OK is returned (indicates the packet has been handled)
                        if (driver->device.flags & JD_DEVICE_FLAGS_BROADCAST || driver->handlePacket(pkt) == DEVICE_OK)
                            break;
                    }
                }
            }

            // iterate over the array again, this time matching on class instead of address, checking for any BROADCAST drivers.
            for (int i = 0; i < JD_PROTOCOL_DRIVER_ARRAY_SIZE; i++)
            {
                JDDriver* driver = this->drivers[i];

                if (driver && driver->device.driver_class == driver_class && driver->device.flags & JD_DEVICE_FLAGS_BROADCAST)
                {
                    JD_DMESG("HANDLED BY BROADCAST");
                    driver->handlePacket(pkt);
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

    memset(this->drivers, 0, sizeof(JDDriver*) * JD_PROTOCOL_DRIVER_ARRAY_SIZE);

    add(logic);

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
            target_enable_irq();
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

int JDProtocol::setBridge(JDDriver* bridge)
{
    this->bridge = bridge;
    return DEVICE_OK;
}

int JDProtocol::send(JDPacket* pkt)
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

int JDProtocol::setDebugName(ManagedString s)
{
    if (s.length() > JD_CONTROL_PACKET_ERROR_NAME_LENGTH)
        return DEVICE_INVALID_PARAMETER;

    memcpy(jacdac_name, s.toCharArray(), s.length());
    return DEVICE_OK;
}

ManagedString JDProtocol::getDebugName()
{
    return ManagedString(jacdac_name, JD_CONTROL_PACKET_ERROR_NAME_LENGTH);
}

void JDProtocol::logState(JackRouter* jr)
{
    if (JDProtocol::instance == NULL)
        return;

    DMESG("Enabled: %d", JDProtocol::instance->bus.isRunning());


    JDBusState busState = JDProtocol::instance->bus.getState();

    const char* busStateStr = "";

    switch(busState)
    {
        case JDBusState::Receiving:
            busStateStr = "Receiving";
            break;

        case JDBusState::Transmitting:
            busStateStr = "Transmitting";
            break;

        case JDBusState::High:
            busStateStr = "High";
            break;

        case JDBusState::Low:
            busStateStr = "Low";
            break;
    }

    DMESG("Bus state: %s", busStateStr);

    const char* jackRouterStateStr = "";

    if (jr)
    {
        JackState s = jr->getState();

        switch (s)
        {
            case JackState::None:
                jackRouterStateStr = "None";
                break;
            case JackState::AllDown:
                jackRouterStateStr = "AllDown";
                break;
            case JackState::HeadPhones:
                jackRouterStateStr = "HeadPhones";
                break;
            case JackState::Buzzer:
                jackRouterStateStr = "Buzzer";
                break;
            case JackState::BuzzerAndSerial:
                jackRouterStateStr = "BuzzerAndSerial";
                break;
        }

        DMESG("Router state: %s", jackRouterStateStr);
    }

    for (int i = 0; i < JD_PROTOCOL_DRIVER_ARRAY_SIZE; i++)
    {
        JDDriver* current = JDProtocol::instance->drivers[i];

        if (current)
            DMESG("Driver %d initialised[%d] address[%d] serial[%d] class[%d], mode[%s%s%s]", i, current->isConnected(), current->device.address, current->device.serial_number, current->device.driver_class, current->device.flags & JD_DEVICE_FLAGS_BROADCAST ? "B" : "", current->device.flags & JD_DEVICE_FLAGS_LOCAL ? "L" : "", current->device.flags & JD_DEVICE_FLAGS_REMOTE ? "R" : "");
    }
}
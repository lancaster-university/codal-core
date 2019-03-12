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

#warning should be moved to control service
static char jacdac_name[JD_MAX_DEVICE_NAME_LENGTH] = {'J', 'A', 'C', 'D', 'A', 'C',' ',' '};

using namespace codal;

JDService* JDProtocol::services[JD_PROTOCOL_SERVICE_ARRAY_SIZE] = { 0 };

JDProtocol* JDProtocol::instance = NULL;

void JDProtocol::onPacketReceived(Event)
{
    JDPacket* pkt = NULL;

    while((pkt = bus.getPacket()) != NULL)
    {
        JD_DMESG("pkt REC AD: %d SZ:%d",pkt->address, pkt->size);

        uint32_t service_class = 0;

        // check if this packet is destined for our services...
        // address 0 will never be filtered.
        if (!control.filterPacket(pkt->device_address))
        {
            JD_DMESG("NOT FILTERED");
            for (int i = 0; i < JD_PROTOCOL_SERVICE_ARRAY_SIZE; i++)
            {
                JDService* service = this->services[i];

                if (service)
                {
                    // the above could be optimised into a single if, but useful for debugging.
                    JD_DMESG("DRIV a:%d sn:%d c:%d i:%d f %d", service->state.device_address, service->state.serial_number, service->state.service_class, service->state.flags & JD_DEVICE_FLAGS_INITIALISED ? 1 : 0, service->state.flags);

                    // if the address is the same, or we're matching on class...
                    if ((service->state.flags & JD_SERVICE_STATE_FLAGS_INITIALISED) && service->state.device_address == pkt->device_address && service->state.service_number == pkt->service_number)
                    {
                        service_class = service->state.service_class;

                        // break if the state is a broadcast to prevent duplicate receptions
                        // break if DEVICE_OK is returned (indicates the packet has been handled)
                        if (service->state.flags & JD_SERVICE_STATE_FLAGS_BROADCAST || service->handlePacket(pkt) == DEVICE_OK)
                            break;
                    }
                }
            }

            // iterate over the array again, this time matching on class instead of address, checking for any BROADCAST services.
            for (int i = 0; i < JD_PROTOCOL_SERVICE_ARRAY_SIZE; i++)
            {
                JDService* service = this->services[i];

                if (service && service->state.service_class == service_class && service->state.flags & JD_SERVICE_STATE_FLAGS_BROADCAST)
                {
                    JD_DMESG("HANDLED BY BROADCAST");
                    service->handlePacket(pkt);
                }
            }
        }

        if (bridge != NULL)
            bridge->handlePacket(pkt);

        free(pkt);
    }
}

JDProtocol::JDProtocol(JACDAC& jacdac, ManagedString name, uint16_t id) : control(), bridge(NULL), bus(jacdac)
{
    this->id = id;

    setDeviceName(name);

    if (instance == NULL)
        instance = this;

    memset(this->services, 0, sizeof(JDService*) * JD_PROTOCOL_SERVICE_ARRAY_SIZE);

    add(control);

    // packets are queued, and should be processed in normal context.
    if (EventModel::defaultEventBus)
        EventModel::defaultEventBus->listen(jacdac.id, JD_SERIAL_EVT_DATA_READY, this, &JDProtocol::onPacketReceived);
}

int JDProtocol::add(JDService& service)
{
    int i;

    // check for duplicates first
    for (i = 0; i < JD_PROTOCOL_SERVICE_ARRAY_SIZE; i++)
        if (services[i] == &service)
            return DEVICE_OK;

    for (i = 0; i < JD_PROTOCOL_SERVICE_ARRAY_SIZE; i++)
    {
        target_disable_irq();
        if (services[i] == NULL)
        {
            services[i] = &service;
            target_enable_irq();
            break;
        }
        target_enable_irq();
    }

    if (i == JD_PROTOCOL_SERVICE_ARRAY_SIZE)
        return DEVICE_NO_RESOURCES;

    return DEVICE_OK;
}

int JDProtocol::remove(JDService& service)
{
    target_disable_irq();
    for (int i = 0; i < JD_PROTOCOL_SERVICE_ARRAY_SIZE; i++)
    {
        if (services[i] == &service)
        {
            services[i] = NULL;
            break;
        }
    }
    target_enable_irq();

    return DEVICE_OK;
}

int JDProtocol::setBridge(JDService* bridge)
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

int JDProtocol::setDeviceName(ManagedString s)
{
    if (s.length() > JD_MAX_DEVICE_NAME_LENGTH)
        return DEVICE_INVALID_PARAMETER;

    memcpy(jacdac_name, s.toCharArray(), s.length());
    return DEVICE_OK;
}

ManagedString JDProtocol::getDeviceName()
{
    return ManagedString(jacdac_name, JD_MAX_DEVICE_NAME_LENGTH);
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

    for (int i = 0; i < JD_PROTOCOL_SERVICE_ARRAY_SIZE; i++)
    {
        JDService* current = JDProtocol::instance->services[i];

        if (current)
            DMESG("Driver %d initialised[%d] address[%d] serial[%d] class[%d], mode[%s%s%s]", i, current->isConnected(), current->state.device_address, current->state.serial_number, current->state.service_class, current->state.flags & JD_SERVICE_STATE_FLAGS_BROADCAST ? "B" : "", current->state.flags & JD_SERVICE_STATE_FLAGS_HOST ? "H" : "", current->state.flags & JD_SERVICE_STATE_FLAGS_CLIENT ? "C" : "");
    }
}
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
#include "JACDAC.h"
#include "Timer.h"
#include "CodalDmesg.h"

using namespace codal;

JDService* JACDAC::services[JD_SERVICE_ARRAY_SIZE] = { 0 };

JACDAC* JACDAC::instance = NULL;

void JACDAC::onPacketReceived(Event)
{
    JDPacket* pkt = NULL;

    while((pkt = bus.getPacket()) != NULL)
    {
        controlService.routePacket(pkt);

        // if we have a bridge service, route all packets to it.
        if (bridge)
            bridge->handlePacket(pkt);

        free(pkt);
    }
}

JACDAC::JACDAC(JDPhysicalLayer& bus, ManagedString name, uint16_t id) : controlService(name), bridge(NULL), bus(bus)
{
    this->id = id;

    if (instance == NULL)
        instance = this;

    memset(this->services, 0, sizeof(JDService*) * JD_SERVICE_ARRAY_SIZE);

    add(controlService);

    // packets are queued, and should be processed in normal context.
    if (EventModel::defaultEventBus)
        EventModel::defaultEventBus->listen(bus.id, JD_SERIAL_EVT_DATA_READY, this, &JACDAC::onPacketReceived);
}

int JACDAC::add(JDService& service)
{
    int i;

    // check for duplicates first
    for (i = 0; i < JD_SERVICE_ARRAY_SIZE; i++)
        if (services[i] == &service)
            return DEVICE_OK;

    for (i = 0; i < JD_SERVICE_ARRAY_SIZE; i++)
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

    if (i == JD_SERVICE_ARRAY_SIZE)
        return DEVICE_NO_RESOURCES;

    return DEVICE_OK;
}

int JACDAC::remove(JDService& service)
{
    target_disable_irq();
    for (int i = 0; i < JD_SERVICE_ARRAY_SIZE; i++)
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

int JACDAC::setBridge(JDService* bridge)
{
    this->bridge = bridge;
    return DEVICE_OK;
}

int JACDAC::send(JDPacket* pkt)
{
    if (instance)
        return instance->bus.send(pkt);

    return DEVICE_NO_RESOURCES;
}

int JACDAC::setDeviceName(ManagedString s)
{
    if (JACDAC::instance == NULL)
        return DEVICE_INVALID_STATE;

    return JACDAC::instance->controlService.setDeviceName(s);
}

ManagedString JACDAC::getDeviceName()
{
    if (JACDAC::instance == NULL)
        return ManagedString();

    return JACDAC::instance->controlService.getDeviceName();
}

void JACDAC::logState()
{
    if (JACDAC::instance == NULL)
        return;

    DMESG("Enabled: %d", JACDAC::instance->bus.isRunning());


    JDBusState busState = JACDAC::instance->bus.getState();

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

    for (int i = 0; i < JD_SERVICE_ARRAY_SIZE; i++)
    {
        JDService* current = JACDAC::instance->services[i];

        if (current)
            DMESG("Driver %d initialised[%d] device_address[%d] serial[%d] class[%d], mode[%s%s%s]", i, current->isConnected(), (current->device) ? current->device->device_address : -1, (current->device) ? current->device->unique_device_identifier : -1, current->service_class, current->mode == BroadcastHostService ? "B" : "", current->mode == HostService ? "H" : "", current->mode == ClientService ? "C" : "");
    }
}

int JACDAC::start()
{
    if (this->status & JD_STARTED)
        return DEVICE_OK;

    controlService.enumerate();

    bus.start();

    this->status |= JD_STARTED;
    return DEVICE_OK;
}

int JACDAC::stop()
{
    if (!(this->status & JD_STARTED))
        return DEVICE_OK;

    bus.stop();

    controlService.disconnect();

    this->status &= ~JD_STARTED;
    return DEVICE_OK;
}

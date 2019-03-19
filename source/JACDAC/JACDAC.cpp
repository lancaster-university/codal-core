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

JDService* JACDAC::services[JD_PROTOCOL_SERVICE_ARRAY_SIZE] = { 0 };

JACDAC* JACDAC::instance = NULL;

void JACDAC::onPacketReceived(Event)
{
    JDPacket* pkt = NULL;

    while((pkt = bus.getPacket()) != NULL)
    {
        JD_DMESG("pkt REC AD: %d sno: %d SZ:%d",pkt->device_address, pkt->service_number, pkt->size);

        if (pkt->device_address == 0)
            controlService.handlePacket(pkt);
        else
        {
            JDDevice* device = controlService.getRemoteDevice(pkt->device_address);
            uint32_t broadcast_class = 0;

            if (!device)
                continue;

            // map from device broadcast map to potentially the service number of one of our enumerated broadcast hosts
            uint16_t host_service_number = device->broadcast_servicemap[pkt->service_number / 2];

            if (pkt->service_number % 2 == 0)
                host_service_number &= 0x0F;
            else
                host_service_number &= 0xF0 >> 4;

            if (!host_service_number)
                host_service_number = JD_SERVICE_NUMBER_UNITIALISED_VAL;

            // handle initialised services
            for (int i = 0; i < JD_PROTOCOL_SERVICE_ARRAY_SIZE; i++)
            {
                JDService* service = this->services[i];

                if (service && service->device == device && service->service_number == pkt->service_number)
                {
                    JD_DMESG("DRIV a:%d sn:%d c:%d i:%d f %d", service->state.device_address, service->state.serial_number, service->state.service_class, service->state.flags & JD_DEVICE_FLAGS_INITIALISED ? 1 : 0, service->state.flags);

                    if (service->device == this->controlService.device && service->service_number == host_service_number)
                    {
                        broadcast_class = service->service_class;
                        continue;
                    }

                    // break if DEVICE_OK is returned (indicates the packet has been handled)
                    if (service->handlePacket(pkt) == DEVICE_OK)
                        break;
                }
            }

            // we matched a broadcast host, route to all broadcast hosts on the device.
            if (broadcast_class)
            {
                for (int i = 0; i < JD_PROTOCOL_SERVICE_ARRAY_SIZE; i++)
                {
                    JDService* service = this->services[i];

                    if (service && service->service_class == broadcast_class && service->mode == BroadcastHostService)
                    {
                        // break if DEVICE_OK is returned (indicates the packet has been handled)
                        if (service->handlePacket(pkt) == DEVICE_OK)
                            break;
                    }
                }
            }

        }

        if (bridge)
            bridge->handlePacket(pkt);

        free(pkt);
    }
}

JACDAC::JACDAC(JDPhysicalLayer& bus, ManagedString name, uint16_t id) : controlService(name), bridge(NULL), bus(bus)
{
    this->id = id;

    setDeviceName(name);

    if (instance == NULL)
        instance = this;

    memset(this->services, 0, sizeof(JDService*) * JD_PROTOCOL_SERVICE_ARRAY_SIZE);

    add(controlService);

    // packets are queued, and should be processed in normal context.
    if (EventModel::defaultEventBus)
        EventModel::defaultEventBus->listen(bus.id, JD_SERIAL_EVT_DATA_READY, this, &JACDAC::onPacketReceived);
}

int JACDAC::add(JDService& service)
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

int JACDAC::remove(JDService& service)
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

    for (int i = 0; i < JD_PROTOCOL_SERVICE_ARRAY_SIZE; i++)
    {
        JDService* current = JACDAC::instance->services[i];

        if (current)
            DMESG("Driver %d initialised[%d] device_address[%d] serial[%d] class[%d], mode[%s%s%s]", i, current->isConnected(), (current->device) ? current->device->device_address : -1, (current->device) ? current->device->udid : -1, current->service_class, current->mode == BroadcastHostService ? "B" : "", current->mode == HostService ? "H" : "", current->mode == ClientService ? "C" : "");
    }
}

int JACDAC::start()
{
    if (this->status & JD_STARTED)
        return DEVICE_OK;

    bus.start();

    controlService.enumerate();

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

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
#include "JDService.h"
#include "CodalDmesg.h"
#include "codal_target_hal.h"
#include "EventModel.h"
#include "CodalFiber.h"
#include "JACDAC.h"

using namespace codal;

uint32_t JDService::dynamicId = DEVICE_ID_JD_DYNAMIC_ID;

int JDService::addAdvertisementData(uint8_t* data)
{
    // by default, the control service will fill in the required information.
    // any additional information should be added here....
    return 0;
}

int JDService::send(uint8_t* buf, int len)
{
    if (JACDAC::instance && this->device)
        return JACDAC::instance->bus.send(buf, len, this->device->device_address, this->service_number, (JDBaudRate)this->device->communication_rate);

    return DEVICE_NO_RESOURCES;
}

JDService::JDService(uint32_t service_class, JDServiceMode m)
{
    // we use a dynamic id for the message bus for simplicity.
    // with the dynamic nature of JACDAC, it would be hard to maintain a consistent id.
    this->id = dynamicId++;
    this->mode = m;
    this->device = NULL;
    this->requiredDevice = NULL;
    this->service_class = service_class;
    this->service_number = JD_SERVICE_NUMBER_UNITIALISED_VAL;

    if (JACDAC::instance)
        JACDAC::instance->add(*this);
}

bool JDService::isConnected()
{
    return (this->status & JD_SERVICE_STATUS_FLAGS_INITIALISED) ? true : false;
}

int JDService::hostConnected()
{
    // DMESG("CONNB a:%d sn:%d cl:%d",device.address,device.serial_number, device.service_class);
    this->status |= JD_SERVICE_STATUS_FLAGS_INITIALISED;
    Event(this->id, JD_SERVICE_EVT_CONNECTED);
    return DEVICE_OK;
}

int JDService::hostDisconnected()
{
    // DMESG("DISCB a:%d sn:%d cl: %d",device.address,device.serial_number, device.service_class);
    this->status &= ~(JD_SERVICE_STATUS_FLAGS_INITIALISED);
    Event(this->id, JD_SERVICE_EVT_DISCONNECTED);
    return DEVICE_OK;
}

JDDevice* JDService::getHostDevice()
{
    return this->device;
}

uint32_t JDService::getServiceClass()
{
    return this->service_class;
}

int JDService::handleServiceInformation(JDDevice* device, JDServiceInformation* info)
{
    return DEVICE_OK;
}

int JDService::handlePacket(JDPacket* p)
{
    return DEVICE_OK;
}

JDService::~JDService()
{
    JACDAC::instance->remove(*this);
}
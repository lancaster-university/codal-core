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
#include "JDProtocol.h"
#include "CodalDmesg.h"
#include "codal_target_hal.h"

using namespace codal;

int JDDriver::fillControlPacket(JDPkt*)
{
    // by default, the logic driver will fill in the required information.
    // any additional information should be added here.... (note: cast pkt->data to control packet and fill out data)
    return DEVICE_OK;
}

JDDriver::JDDriver(JDDevice d, uint16_t id) : device(d)
{
    this->id = id;

    if (JDProtocol::instance)
        JDProtocol::instance->add(*this);
}

bool JDDriver::isConnected()
{
    return (this->device.flags & JD_DEVICE_FLAGS_INITIALISED) ? true : false;
}

int JDDriver::deviceConnected(JDDevice device)
{
    DMESG("CONNECTED a:%d sn:%d",device.address,device.serial_number);
    uint16_t flags = this->device.flags & 0xFF00;
    this->device = device;
    this->device.flags = (flags | JD_DEVICE_FLAGS_INITIALISED | JD_DEVICE_FLAGS_CP_SEEN);
    Event(this->id, JD_DRIVER_EVT_CONNECTED);
    return DEVICE_OK;
}

int JDDriver::deviceRemoved()
{
    DMESG("DISCONN a:%d sn:%d",device.address,device.serial_number);
    this->device.flags &= ~(JD_DEVICE_FLAGS_INITIALISED);
    this->device.rolling_counter = 0;
    Event(this->id, JD_DRIVER_EVT_DISCONNECTED);
    return DEVICE_OK;
}

int JDDriver::handleControlPacket(JDPkt* p)
{
    return DEVICE_CANCELLED;
}

int JDDriver::handlePacket(JDPkt* p)
{
    return DEVICE_CANCELLED;
}

JDDriver::~JDDriver()
{
    JDProtocol::instance->remove(*this);
}
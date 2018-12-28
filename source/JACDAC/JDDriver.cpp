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
#include "EventModel.h"
#include "CodalFiber.h"

using namespace codal;

uint32_t JDDriver::dynamicId = DEVICE_ID_JD_DYNAMIC_ID;

int JDDriver::fillControlPacket(JDPkt*)
{
    // by default, the logic driver will fill in the required information.
    // any additional information should be added here.... (note: cast pkt->data to control packet and fill out data)
    return DEVICE_OK;
}

void JDDriver::pair()
{
    // create a pairing request control packet.
    ControlPacket cp;
    cp.address = this->pairedInstance->getAddress();
    cp.driver_class = this->pairedInstance->getClass();
    cp.serial_number = this->pairedInstance->getSerialNumber();
    cp.packet_type = JD_CONTROL_TYPE_PAIRING_REQUEST;

    // put the source address (our address) into the packet (should have plenty of room in a control packet)
    memcpy(cp.data, (uint8_t*)&this->device, sizeof(JDDevice));

    DMESG("SEND PAIRING REQ: A %d S %d", cp.address, cp.serial_number);

    // address the packet to the logic driver.
    JDProtocol::send((uint8_t*)&cp, sizeof(ControlPacket), 0);
}

int JDDriver::sendPairingPacket(JDDevice d)
{
    d.flags |= JD_DEVICE_FLAGS_INITIALISED;
    // send pairing request should create the paired instance, flag pairing mode, and swap to local mode to get an address.
    // Once enumerated we then send the packet.
    this->pairedInstance = new JDPairedDriver(d, *this);

    if (EventModel::defaultEventBus)
        EventModel::defaultEventBus->listen(pairedInstance->id, JD_DRIVER_EVT_DISCONNECTED, this, &JDDriver::partnerDisconnected);

    // set our mode to pairing mode (this mode is not accessible as an api).
    this->device.setMode((DriverType)(JD_DEVICE_FLAGS_LOCAL | JD_DEVICE_FLAGS_PAIR | JD_DEVICE_FLAGS_PAIRING));

    return DEVICE_OK;
}

int JDDriver::handleLogicPacket(JDDriverInfo* info)
{
    // filter out any pairing requests for special handling by drivers.
    if (info->type == JD_CONTROL_TYPE_PAIRING_REQUEST)
        return this->handlePairingInfo(info);

    // lower four bits of flags are used for error codes.
    if (info->flags & 0x000F)
        return this->handleErrorInfo(info);

    return this->handleControlInfo(info);
}

JDDriver::JDDriver(JDDevice d) : device(d)
{
    // we use a dynamic id for the message bus for simplicity.
    // with the dynamic nature of JACDAC, it would be hard to maintain a consistent id.
    this->id = dynamicId++;

    this->pairedInstance = NULL;

    if (JDProtocol::instance)
        JDProtocol::instance->add(*this);
}

bool JDDriver::isConnected()
{
    return (this->device.flags & JD_DEVICE_FLAGS_INITIALISED) ? true : false;
}

int JDDriver::deviceConnected(JDDevice device)
{
    // DMESG("CONNB a:%d sn:%d cl:%d",device.address,device.serial_number, device.driver_class);
    this->device.address = device.address;
    this->device.serial_number = device.serial_number;
    this->device.flags |= JD_DEVICE_FLAGS_INITIALISED | JD_DEVICE_FLAGS_CP_SEEN;

    // if we are connecting and in pairing mode, we should invoke pair, the second stage of sendPairingPacket().
    if (this->device.isPairing())
        this->pair();

    Event(this->id, JD_DRIVER_EVT_CONNECTED);
    return DEVICE_OK;
}

int JDDriver::deviceRemoved()
{
    // DMESG("DISCB a:%d sn:%d cl: %d",device.address,device.serial_number, device.driver_class);
    this->device.flags &= ~(JD_DEVICE_FLAGS_INITIALISED);
    this->device.rolling_counter = 0;
    Event(this->id, JD_DRIVER_EVT_DISCONNECTED);
    return DEVICE_OK;
}

int JDDriver::handlePairingPacket(JDDriverInfo* p)
{
    DMESG("Pair PKT");
    #warning fix pairing
    // take a local copy of the control packet (don't modify the one we are given as it will be passed onto the next driver.)
    // control packet data for a pairing packet contains the source address of the partner
    JDDevice d = *((JDDevice*)p->data);

    // // if the packet is addressed to us
    // if (this->device.serial_number == cp.serial_number)
    // {
    //     // if we requested to pair
    //     if (this->device.isPairing())
    //     {
    //         DMESG("PAIRING RESPONSE");
    //         this->device.flags &= ~JD_DEVICE_FLAGS_PAIRING;

    //         if (cp.flags & JD_CONTROL_FLAGS_NACK)
    //         {
    //             DMESG("PAIRING REQ DENIED", d.address, d.serial_number);
    //             if (this->pairedInstance)
    //             {
    //                 delete this->pairedInstance;
    //                 this->pairedInstance = NULL;
    //             }

    //             this->device.setMode(PairedDriver);
    //             Event(this->id, JD_DRIVER_EVT_PAIR_REJECTED);
    //         }
    //         else if (cp.flags & JD_CONTROL_FLAGS_ACK)
    //         {
    //             DMESG("PAIRING REQ ACK", d.address, d.serial_number);
    //             this->device.flags |= JD_DEVICE_FLAGS_PAIRED;
    //             Event(this->id, JD_DRIVER_EVT_PAIRED);
    //         }

    //         return DEVICE_OK;
    //     }

    //     // we may reply using the same control packet for ease.
    //     // populate similar fields.
    //     cp.address = d.address;
    //     cp.serial_number = d.serial_number;
    //     cp.driver_class = d.driver_class;

    //     // copy our device data into the packet for any additional checking (not required at the moment)
    //     memcpy(cp.data, (uint8_t*)&this->device, sizeof(JDDevice)); // should have plenty of room in a control packet

    //     // if we are able to pair...
    //     if (this->device.isPairable())
    //     {
    //         // respond with a packet DIRECTED at the device that sent us the pairing request
    //         cp.flags |= JD_CONTROL_FLAGS_ACK;
    //         JDProtocol::send((uint8_t*)&cp, sizeof(ControlPacket), 0);

    //         DMESG("PAIRING REQ: A %d S %d", d.address, d.serial_number);
    //         // update our flags
    //         this->device.flags &= ~JD_DEVICE_FLAGS_PAIRABLE;
    //         this->device.flags |= JD_DEVICE_FLAGS_PAIRED;

    //         // create a local instance of a remote device so that if the device is disconnected we are informed.
    //         d.flags = JD_DEVICE_FLAGS_REMOTE | JD_DEVICE_FLAGS_INITIALISED;
    //         this->pairedInstance = new JDPairedDriver(d, *this);

    //         // listen for disconnection events.
    //         if (EventModel::defaultEventBus)
    //             EventModel::defaultEventBus->listen(pairedInstance->id, JD_DRIVER_EVT_DISCONNECTED, this, &JDDriver::partnerDisconnected);

    //         // let applications know we have paired.
    //         Event(this->id, JD_DRIVER_EVT_PAIRED);
    //         DMESG("PAIRING DONE");

    //         return DEVICE_OK;
    //     }
    //     // explicity been asked to unpair.
    //     else if (device.isPaired() && pairedInstance->device.serial_number == d.serial_number && cp.flags & JD_CONTROL_FLAGS_NACK)
    //     {
    //         Event e(0, 0, 0, CREATE_ONLY);
    //         partnerDisconnected(e);
    //     }
    //     else if (device.flags & JD_DEVICE_FLAGS_PAIR)
    //     {
    //         // nack only if we're capable of being paired
    //         DMESG("NACK A %d S %d", d.address, d.serial_number);

    //         // respond with a packet DIRECTED at the device that sent us the pairing request
    //         cp.flags |= JD_CONTROL_FLAGS_NACK;
    //         JDProtocol::send((uint8_t*)&cp, sizeof(ControlPacket), 0);
    //         return DEVICE_OK;
    //     }

    // }

    return DEVICE_CANCELLED;
}

bool JDDriver::isPaired()
{
    return device.isPaired();
}

bool JDDriver::isPairable()
{
    return device.isPairable();
}

uint8_t JDDriver::getAddress()
{
    return device.address;
}

uint32_t JDDriver::getClass()
{
    return device.driver_class;
}

JDDevice JDDriver::getState()
{
    return device;
}

uint32_t JDDriver::getSerialNumber()
{
    return device.serial_number;
}

void JDDriver::partnerDisconnected(Event)
{
    DMESG("PARTNER D/C");
    EventModel::defaultEventBus->ignore(pairedInstance->id, JD_DRIVER_EVT_DISCONNECTED, this, &JDDriver::partnerDisconnected);

    this->device.flags &= ~JD_DEVICE_FLAGS_PAIRED;

    // return to our correct defaults:
    // * pairing mode for a PairedDriver
    // * advertise pairable for a PairableHostDriver.
    if (this->device.flags & JD_DEVICE_FLAGS_PAIR)
        this->device.setMode(PairedDriver);
    else
        this->device.flags |= JD_DEVICE_FLAGS_PAIRABLE;

    // we should have a paired instance to reach, but double check just in case
    if (this->pairedInstance)
    {
        target_disable_irq();
        delete this->pairedInstance;
        this->pairedInstance = NULL;
        target_enable_irq();
    }

    // signal that we have lost our partner.
    Event(this->id, JD_DRIVER_EVT_UNPAIRED);
}

int JDDriver::handleControlPacket(JDPkt* p)
{
    return DEVICE_OK;
}

int JDDriver::handleErrorPacket(JDPkt* p)
{
    ControlPacket* cp = (ControlPacket*)p->data;
    ControlPacketError* err = (ControlPacketError*)cp->data;
    this->device.setError((DriverErrorCode)err->code);
    Event(this->id, JD_DRIVER_EVT_ERROR);
    return DEVICE_OK;
}

int JDDriver::handlePacket(JDPkt* p)
{
    return DEVICE_OK;
}

JDDriver::~JDDriver()
{
    JDProtocol::instance->remove(*this);

    if (this->pairedInstance)
        delete this->pairedInstance;
}
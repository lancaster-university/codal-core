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
#include "JDControlLayer.h"
#include "CodalDmesg.h"
#include "codal_target_hal.h"
#include "EventModel.h"
#include "CodalFiber.h"

using namespace codal;

uint32_t JDService::dynamicId = DEVICE_ID_JD_DYNAMIC_ID;

int JDService::populateServiceInfo(JDServiceInfo*, uint8_t)
{
    // by default, the control service will fill in the required information.
    // any additional information should be added here....
    return 0;
}

int JDService::send(uint8_t* buf, int len)
{
    if (JDControlLayer::instance)
        return JDControlLayer::instance->bus.send(buf, len, this->state.device_address, this->state.service_number, this->state.getBaudRate());

    return DEVICE_NO_RESOURCES;
}

void JDService::pair()
{
    // // create a pairing request control packet.
    // int packetSize = JD_CONTROL_PACKET_HEADER_SIZE + JD_SERVICE_STATE_HEADER_SIZE + sizeof(JDServiceState);
    // JDControlPacket* cp = (JDControlPacket *)malloc(packetSize);
    // JDServiceInfo* info = (JDServiceInfo *)cp->data;

    // info->address = this->pairedInstance->getAddress();
    // info->service_class = this->pairedInstance->getClass();
    // cp->serial_number = this->pairedInstance->getSerialNumber();
    // info->type = JD_SERVICE_STATE_TYPE_PAIRING_REQUEST;
    // info->size = sizeof(JDServiceState);

    // // put the source address (our address) into the packet (should have plenty of room in a control packet)
    // memcpy(info->data, (uint8_t*)&this->state, sizeof(JDServiceState));

    // DMESG("SEND PAIRING REQ: A %d S %d", info->address, cp->serial_number);

    // // address the packet to the control service.
    // send((uint8_t*)cp, packetSize);
}

int JDService::sendPairingPacket(JDServiceState d)
{
    // d.flags |= JD_SERVICE_STATE_FLAGS_INITIALISED;
    // // send pairing request should create the paired instance, flag pairing mode, and swap to local mode to get an address.
    // // Once enumerated we then send the packet.
    // this->pairedInstance = new JDPairedService(d, *this);

    // if (EventModel::defaultEventBus)
    //     EventModel::defaultEventBus->listen(pairedInstance->id, JD_SERVICE_EVT_DISCONNECTED, this, &JDService::partnerDisconnected);

    // // set our mode to pairing mode (this mode is not accessible as an api).
    // this->state.setMode((ServiceType)(JD_SERVICE_STATE_FLAGS_HOST | JD_SERVICE_STATE_FLAGS_PAIR | JD_SERVICE_STATE_FLAGS_PAIRING));

    return DEVICE_OK;
}

int JDService::handleLogicPacket(JDControlPacket* cp)
{
    JDServiceInfo* info = (JDServiceInfo *)cp->data;
    int ret = DEVICE_OK;

    // filter out any pairing requests for special handling by services.
    // if (info->type == JD_SERVICE_STATE_TYPE_PAIRING_REQUEST)
    //     ret = this->handlePairingPacket(cp);

    // // if an error is present invoke the error handler instead
    // else if (info->error_code > 0)
    //     ret = this->handleErrorPacket(cp);

    // else
        ret = this->handleControlPacket(cp);

    return ret;
}

JDService::JDService(JDServiceState state) : state(state)
{
    // we use a dynamic id for the message bus for simplicity.
    // with the dynamic nature of JACDAC, it would be hard to maintain a consistent id.
    this->id = dynamicId++;

    this->pairedInstance = NULL;

    if (JDControlLayer::instance)
        JDControlLayer::instance->add(*this);
}

bool JDService::isConnected()
{
    return (this->state.flags & JD_SERVICE_STATE_FLAGS_INITIALISED) ? true : false;
}

int JDService::deviceConnected(JDServiceState device)
{
    // DMESG("CONNB a:%d sn:%d cl:%d",device.address,device.serial_number, device.service_class);
    this->state.device_address = device.device_address;
    this->state.serial_number = device.serial_number;
    this->state.flags |= JD_SERVICE_STATE_FLAGS_INITIALISED | JD_SERVICE_STATE_FLAGS_CP_SEEN;

    // if we are connecting and in pairing mode, we should invoke pair, the second stage of sendPairingPacket().
    if (this->state.isPairing())
        this->pair();

    Event(this->id, JD_SERVICE_EVT_CONNECTED);
    return DEVICE_OK;
}

int JDService::deviceRemoved()
{
    // DMESG("DISCB a:%d sn:%d cl: %d",device.address,device.serial_number, device.service_class);
    this->state.flags &= ~(JD_SERVICE_STATE_FLAGS_INITIALISED);
    this->state.rolling_counter = 0;
    Event(this->id, JD_SERVICE_EVT_DISCONNECTED);
    return DEVICE_OK;
}

int JDService::handlePairingPacket(JDControlPacket* cp)
{
    DMESG("Pair PKT");
    // JDServiceInfo* info = (JDServiceInfo *)cp->data;

    // // take a local copy of the control packet (don't modify the one we are given as it will be passed onto the next service.)
    // // control packet data for a pairing packet contains the source address of the partner
    // JDServiceState d = *((JDServiceState*)info->data);

    // // if the packet is addressed to us
    // if (this->state.serial_number == cp->serial_number)
    // {
    //     // if we requested to pair
    //     if (this->state.isPairing())
    //     {
    //         DMESG("PAIRING RESPONSE");
    //         this->state.flags &= ~JD_SERVICE_STATE_FLAGS_PAIRING;

    //         if (info->flags & JD_SERVICE_STATE_FLAGS_NACK)
    //         {
    //             DMESG("PAIRING REQ DENIED", d.address, d.serial_number);
    //             if (this->pairedInstance)
    //             {
    //                 delete this->pairedInstance;
    //                 this->pairedInstance = NULL;
    //             }

    //             this->state.setMode(PairedService);
    //             Event(this->id, JD_SERVICE_EVT_PAIR_REJECTED);
    //         }
    //         else if (info->flags & JD_SERVICE_STATE_FLAGS_ACK)
    //         {
    //             DMESG("PAIRING REQ ACK", d.address, d.serial_number);
    //             this->state.flags |= JD_SERVICE_STATE_FLAGS_PAIRED;
    //             Event(this->id, JD_SERVICE_EVT_PAIRED);
    //         }

    //         return DEVICE_OK;
    //     }

    //     // we may reply using the same control packet for ease.
    //     // populate similar fields.
    //     info->address = d.address;
    //     cp->serial_number = d.serial_number;
    //     info->service_class = d.service_class;
    //     info->size = sizeof(JDServiceState);

    //     // copy our device data into the packet for any additional checking (not required at the moment)
    //     memcpy(info->data, (uint8_t*)&this->state, sizeof(JDServiceState)); // should have plenty of room in a control packet

    //     // if we are able to pair...
    //     if (this->state.isPairable())
    //     {
    //         // respond with a packet DIRECTED at the device that sent us the pairing request
    //         info->flags |= JD_SERVICE_STATE_FLAGS_ACK;
    //         send((uint8_t*)cp, JD_CONTROL_PACKET_HEADER_SIZE + JD_SERVICE_STATE_HEADER_SIZE + sizeof(JDServiceState));

    //         DMESG("PAIRING REQ: A %d S %d", d.address, d.serial_number);
    //         // update our flags
    //         this->state.flags &= ~JD_SERVICE_STATE_FLAGS_PAIRABLE;
    //         this->state.flags |= JD_SERVICE_STATE_FLAGS_PAIRED;

    //         // create a local instance of a remote device so that if the device is disconnected we are informed.
    //         d.flags = JD_SERVICE_STATE_FLAGS_CLIENT | JD_SERVICE_STATE_FLAGS_INITIALISED;
    //         this->pairedInstance = new JDPairedService(d, *this);

    //         // listen for disconnection events.
    //         if (EventModel::defaultEventBus)
    //             EventModel::defaultEventBus->listen(pairedInstance->id, JD_SERVICE_EVT_DISCONNECTED, this, &JDService::partnerDisconnected);

    //         // let applications know we have paired.
    //         Event(this->id, JD_SERVICE_EVT_PAIRED);
    //         DMESG("PAIRING DONE");

    //         return DEVICE_OK;
    //     }
    //     // explicity been asked to unpair.
    //     else if (this->state.isPaired() && pairedInstance->state.serial_number == d.serial_number && info->flags & JD_SERVICE_STATE_FLAGS_NACK)
    //     {
    //         Event e(0, 0, 0, CREATE_ONLY);
    //         partnerDisconnected(e);
    //     }
    //     else if (this->state.flags & JD_SERVICE_STATE_FLAGS_PAIR)
    //     {
    //         // nack only if we're capable of being paired
    //         DMESG("NACK A %d S %d", d.address, d.serial_number);

    //         // respond with a packet DIRECTED at the device that sent us the pairing request
    //         info->service_flags |= JD_SERVICE_STATE_FLAGS_NACK;
    //         send((uint8_t*)cp, JD_CONTROL_PACKET_HEADER_SIZE + JD_SERVICE_INFO_HEADER_SIZE + sizeof(JDServiceState));
    //         return DEVICE_OK;
    //     }

    // }

    return DEVICE_CANCELLED;
}

bool JDService::isPaired()
{
    return this->state.isPaired();
}

bool JDService::isPairable()
{
    return this->state.isPairable();
}

uint8_t JDService::getAddress()
{
    return this->state.device_address;
}

uint32_t JDService::getClass()
{
    return this->state.service_class;
}

JDServiceState JDService::getState()
{
    return this->state;
}

uint32_t JDService::getSerialNumber()
{
    return this->state.serial_number;
}

void JDService::partnerDisconnected(Event)
{
    DMESG("PARTNER D/C");
    EventModel::defaultEventBus->ignore(pairedInstance->id, JD_SERVICE_EVT_DISCONNECTED, this, &JDService::partnerDisconnected);

    this->state.flags &= ~JD_SERVICE_STATE_FLAGS_PAIRED;

    // return to our correct defaults:
    // * pairing mode for a PairedService
    // * advertise pairable for a PairableHostService.
    if (this->state.flags & JD_SERVICE_STATE_FLAGS_PAIR)
        this->state.setMode(PairedService);
    else
        this->state.flags |= JD_SERVICE_STATE_FLAGS_PAIRABLE;

    // we should have a paired instance to reach, but double check just in case
    if (this->pairedInstance)
    {
        target_disable_irq();
        delete this->pairedInstance;
        this->pairedInstance = NULL;
        target_enable_irq();
    }

    // signal that we have lost our partner.
    Event(this->id, JD_SERVICE_EVT_UNPAIRED);
}

int JDService::handleControlPacket(JDControlPacket* cp)
{
    return DEVICE_OK;
}

int JDService::handleErrorPacket(JDControlPacket* cp)
{
    JDServiceInfo* info = (JDServiceInfo *)cp->data;
    this->state.setError((ServiceErrorCode)info->service_status);
    Event(this->id, JD_SERVICE_EVT_ERROR);
    return DEVICE_OK;
}

int JDService::handlePacket(JDPacket* p)
{
    return DEVICE_OK;
}

JDService::~JDService()
{
    JDControlLayer::instance->remove(*this);

    if (this->pairedInstance)
        delete this->pairedInstance;
}
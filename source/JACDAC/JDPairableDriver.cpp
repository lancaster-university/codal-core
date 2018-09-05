#include "JDPairableDriver.h"
#include "EventModel.h"

using namespace codal;

int JDPairableDriver::fillControlPacket(JDPkt* p)
{
    ControlPacket* cp = (ControlPacket *)p->data;

    if (pairedInstance)
        cp->flags |=  CONTROL_JD_FLAGS_PAIRED;
    else
        cp->flags |= CONTROL_JD_FLAGS_PAIRABLE;

    return DEVICE_OK;
}

JDPairableDriver::JDPairableDriver(JDDevice d, uint16_t id) : JDDriver(d, id)
{
    this->pairedInstance = NULL;
}

int JDPairableDriver::sendPairingRequest(JDPkt* p)
{
    ControlPacket* cp = (ControlPacket *)p->data;
    cp->packet_type = CONTROL_JD_TYPE_PAIRING_REQUEST;
    memcpy(cp->data, (uint8_t*)&this->device, sizeof(JDDevice)); // should have plenty of room in a control packet

    this->pairedInstance = new JDDriver(JDDevice(cp->address, JD_DEVICE_FLAGS_REMOTE | JD_DEVICE_FLAGS_INITIALISED, cp->serial_number, cp->driver_class), this->id);

    if (EventModel::defaultEventBus)
        EventModel::defaultEventBus->listen(pairedInstance->id, JD_DRIVER_EVT_DISCONNECTED, this, &JDPairableDriver::partnerDisconnected);

    JDProtocol::send((uint8_t*)cp, sizeof(ControlPacket), 0);
    return DEVICE_OK;
}

int JDPairableDriver::handlePairingRequest(JDPkt* p)
{
    ControlPacket* cp = (ControlPacket *)p->data;
    JDDevice d = *((JDDevice*)cp->data);

    // we have received a NACK from our pairing request, delete our local representation of our partner.
    if (this->pairedInstance && cp->flags & CONTROL_JD_FLAGS_NACK && this->device.serial_number == cp->serial_number)
    {
        Event e(0,0,CREATE_ONLY);
        partnerDisconnected(e);
    }
    else if (this->device.serial_number == cp->serial_number)
    {
        d.flags = JD_DEVICE_FLAGS_REMOTE | JD_DEVICE_FLAGS_INITIALISED;
        this->pairedInstance = new JDDriver(d, this->id); // id needs handling a little better

        if (EventModel::defaultEventBus)
            EventModel::defaultEventBus->listen(pairedInstance->id, JD_DRIVER_EVT_DISCONNECTED, this, &JDPairableDriver::partnerDisconnected);

        return DEVICE_OK;
    }
    else
    {
        cp->flags |= CONTROL_JD_FLAGS_NACK;
        cp->address = d.address;
        cp->serial_number = d.serial_number;
        cp->driver_class = d.driver_class;

        memcpy(cp->data, (uint8_t*)&this->device, sizeof(JDDevice)); // should have plenty of room in a control packet

        JDProtocol::send((uint8_t*)cp, sizeof(ControlPacket), 0);
        return DEVICE_OK;
    }

    return DEVICE_CANCELLED;
}

void JDPairableDriver::partnerDisconnected(Event)
{
    EventModel::defaultEventBus->ignore(pairedInstance->id, JD_DRIVER_EVT_DISCONNECTED, this, &JDPairableDriver::partnerDisconnected);

    target_disable_irq();
    delete this->pairedInstance;
    this->pairedInstance = NULL;
    target_enable_irq();
}

int JDPairableDriver::handleControlPacket(JDPkt* p)
{
    ControlPacket* cp = (ControlPacket *)p->data;

    if (cp->packet_type  == CONTROL_JD_TYPE_PAIRING_REQUEST)
        return handlePairingRequest(p);

    if (cp->flags & CONTROL_JD_FLAGS_PAIRABLE)
        return sendPairingRequest(p);

    return DEVICE_OK;
}
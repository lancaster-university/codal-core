#include "JDPinDriver.h"
#include "CodalDmesg.h"

using namespace codal;

JDPinDriver::JDPinDriver(Pin& p) : pin(&p), JDDriver(JDDevice(HostDriver, JD_DRIVER_CLASS_PIN), dynamicId++)
{
}

JDPinDriver::JDPinDriver() : pin(NULL), JDDriver(JDDevice(VirtualDriver, JD_DRIVER_CLASS_PIN), dynamicId++)
{
}

int JDPinDriver::sendPacket(Mode m, uint32_t value)
{
    if (!(this->device.flags & JD_DEVICE_FLAGS_REMOTE) || !this->isConnected())
        return DEVICE_INVALID_STATE;

    PinPacket p;
    p.mode = m;
    p.value = value;

    JDProtocol::send((uint8_t*)&p, sizeof(PinPacket), this->device.address);
}

int JDPinDriver::setAnalogValue(uint32_t value)
{
    sendPacket(SetAnalog, value);
}

int JDPinDriver::setDigitalValue(uint32_t value)
{
    sendPacket(SetDigital, value);
}

int JDPinDriver::setServoValue(uint32_t value)
{
    sendPacket(SetServo, value);
}

int JDPinDriver::handleControlPacket(JDPkt* cp)
{
}

int JDPinDriver::handlePacket(JDPkt* p)
{
    PinPacket* pinData = (PinPacket*)p->data;

    if (isPaired() && this->pairedInstance->getAddress() != p->address)
        return DEVICE_OK;

    switch (pinData->mode)
    {
        case SetDigital:
            pin->setDigitalValue(pinData->value);
            break;

        case SetAnalog:
            pin->setAnalogValue(pinData->value);
            break;

        case SetServo:
            pin->setServoValue(pinData->value);
            break;
    }

    return DEVICE_OK;
}
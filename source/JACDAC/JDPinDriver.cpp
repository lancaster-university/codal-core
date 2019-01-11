#include "JDPinDriver.h"
#include "CodalDmesg.h"

using namespace codal;

JDPinDriver::JDPinDriver(Pin& p) : JDDriver(JDDevice(PairableHostDriver, JD_DRIVER_CLASS_PIN)), pin(&p)
{
}

JDPinDriver::JDPinDriver() : JDDriver(JDDevice(PairedDriver, JD_DRIVER_CLASS_PIN)), pin(NULL)
{
}

int JDPinDriver::sendPacket(Mode m, uint32_t value)
{
    if (!(this->device.isPaired()) || !this->isConnected())
        return DEVICE_INVALID_STATE;

    PinPacket p;
    p.mode = m;
    p.value = value;

    return JDProtocol::send((uint8_t*)&p, sizeof(PinPacket), this->device.address);
}

int JDPinDriver::setAnalogValue(uint32_t value)
{
    return sendPacket(SetAnalog, value);
}

int JDPinDriver::setDigitalValue(uint32_t value)
{
    return sendPacket(SetDigital, value);
}

int JDPinDriver::setServoValue(uint32_t value)
{
    return sendPacket(SetServo, value);
}

int JDPinDriver::handleControlPacket(JDControlPacket* cp)
{
    JDDriverInfo* info = (JDDriverInfo*)cp->data;

    DMESG("PIN CONTROL PKT!");

    if (this->device.isPairedDriver() && !this->device.isPaired())
    {
        DMESG("NEED TO PAIR!");
        if (info->flags & JD_DRIVER_INFO_FLAGS_PAIRABLE)
        {
            DMESG("PAIR!");
            sendPairingPacket(JDDevice(info->address, JD_DEVICE_FLAGS_REMOTE | JD_DEVICE_FLAGS_INITIALISED | JD_DEVICE_FLAGS_CP_SEEN, cp->serial_number, info->driver_class));
        }
    }

    return DEVICE_OK;
}

int JDPinDriver::handlePacket(JDPacket* p)
{
    PinPacket* pinData = (PinPacket*)p->data;

    DMESG("PIN DATA: paired %d %d %d",isPaired(), this->pairedInstance->getAddress(), p->address);

    if (device.isVirtualDriver() || (isPaired() && this->pairedInstance->getAddress() != p->address))
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
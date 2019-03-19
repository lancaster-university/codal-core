#include "JDPinService.h"
#include "CodalDmesg.h"

using namespace codal;
#ifdef FOO

JDPinService::JDPinService(Pin& p) : JDService(JD_SERVICE_CLASS_PIN, HostService), pin(&p)
{
}

JDPinService::JDPinService() : JDService(JD_SERVICE_CLASS_PIN, ClientService), pin(NULL)
{
}

int JDPinService::sendPacket(Mode m, uint32_t value)
{
    if (!this->isConnected())
        return DEVICE_INVALID_STATE;

    PinPacket p;
    p.mode = m;
    p.value = value;

    return send((uint8_t*)&p, sizeof(PinPacket));
}

int JDPinService::setAnalogValue(uint32_t value)
{
    return sendPacket(SetAnalog, value);
}

int JDPinService::setDigitalValue(uint32_t value)
{
    return sendPacket(SetDigital, value);
}

int JDPinService::setServoValue(uint32_t value)
{
    return sendPacket(SetServo, value);
}

int JDPinService::handlePacket(JDPacket* p)
{
    PinPacket* pinData = (PinPacket*)p->data;

    if (this->mode == ClientService)
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
#endif
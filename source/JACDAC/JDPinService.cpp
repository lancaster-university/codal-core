#include "JDPinService.h"
#include "CodalDmesg.h"

using namespace codal;

JDPinService::JDPinService(Pin& p) : JDService(JDServiceState(PairableHostService, JD_DRIVER_CLASS_PIN)), pin(&p)
{
}

JDPinService::JDPinService() : JDService(JDServiceState(PairedService, JD_DRIVER_CLASS_PIN)), pin(NULL)
{
}

int JDPinService::sendPacket(Mode m, uint32_t value)
{
    if (!(this->state.isPaired()) || !this->isConnected())
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

int JDPinService::handleControlPacket(JDControlPacket* cp)
{
    #warning pairing broken
    // JDServiceInfo* info = (JDServiceInfo*)cp->data;

    // DMESG("PIN CONTROL PKT!");

    // if (this->state.isPairedDriver() && !this->state.isPaired())
    // {
    //     DMESG("NEED TO PAIR!");
    //     if (info->flags & JD_DEVICE_FLAGS_PAIRABLE)
    //     {
    //         DMESG("PAIR!");
    //         sendPairingPacket(JDServiceState(info->device_address, JD_SERVICE_STATE_FLAGS_CLIENT | JD_SERVICE_STATE_FLAGS_INITIALISED | JD_SERVICE_STATE_FLAGS_CP_SEEN, cp->serial_number, info->service_class));
    //     }
    // }

    return DEVICE_OK;
}

int JDPinService::handlePacket(JDPacket* p)
{
    PinPacket* pinData = (PinPacket*)p->data;

    DMESG("PIN DATA: paired %d %d %d",isPaired(), this->pairedInstance->getAddress(), p->device_address);

    if (state.isClient() || (isPaired() && this->pairedInstance->getAddress() != p->device_address))
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
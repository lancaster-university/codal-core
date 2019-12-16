#include "JDRNGService.h"
#include "JACDAC.h"

using namespace codal;

int JDRNGService::send(uint8_t* buf, int len)
{
    if (JACDAC::instance)
        return JACDAC::instance->bus.send(buf, len, this->service_number, 0, this->device);

    return DEVICE_NO_RESOURCES;
}

JDRNGService::JDRNGService() : JDService(JD_SERVICE_IDENTIFIER_CONTROL_RNG, ControlLayerService)
{
    this->service_number = JD_CONTROL_RNG_SERVICE_NUMBER;
}

int JDRNGService::handlePacket(JDPacket* p)
{
    JDRNGServicePacket* rng = (JDRNGServicePacket*)p->data;
    JDRNGServicePacket resp;

    if (rng->request_type == JD_CONTROL_RNG_SERVICE_REQUEST_TYPE_REQ)
    {
        resp.request_type = JD_CONTROL_RNG_SERVICE_REQUEST_TYPE_RESP;
        resp.random = target_random(0xFFFFFFFF);
        send((uint8_t*)&resp, sizeof(JDRNGServicePacket));
    }

    return DEVICE_OK;
}
#include "JDConfigurationService.h"
#include "JACDAC.h"
#include "CodalDmesg.h"

using namespace codal;

int JDConfigurationService::send(uint8_t* buf, int len)
{
    if (JACDAC::instance)
        return JACDAC::instance->bus.send(buf, len, this->service_number, NULL);

    return DEVICE_NO_RESOURCES;
}

JDConfigurationService::JDConfigurationService(uint16_t id) : JDService(JD_SERVICE_CLASS_CONTROL_CONFIGURATION, ClientService)
{
    this->service_number = JD_CONTROL_CONFIGURATION_SERVICE_NUMBER;
    this->id = id;
}

int JDConfigurationService::handlePacket(JDPacket* p)
{
    JDConfigurationPacket* pkt = (JDConfigurationPacket *)p->data;
    JD_DMESG("CFG size: %d", p->size);

    if (this->device)
    {
        if (pkt->device_address == this->device->device_address)
        {
            if (pkt->request_type == JD_CONTROL_CONFIGURATION_SERVICE_REQUEST_TYPE_NAME)
            {
                uint8_t* namePtr = pkt->data;
                int len = *namePtr++;
                JACDAC::instance->setDeviceName(ManagedString((char *)namePtr, len));
                Event(this->id, JD_CONTROL_CONFIGURATION_EVT_NAME);
            }

            if (pkt->request_type == JD_CONTROL_CONFIGURATION_SERVICE_REQUEST_TYPE_IDENTIFY)
                Event(this->id, JD_CONTROL_CONFIGURATION_EVT_IDENTIFY);
        }
    }

    return DEVICE_OK;
}

int JDConfigurationService::setRemoteDeviceName(uint8_t device_address, ManagedString newName)
{
    int len = newName.length();

    if (len > JD_SERIAL_MAX_PAYLOAD_SIZE)
        return DEVICE_INVALID_PARAMETER;

    int size = JD_CONTROL_CONFIGURATION_SERVICE_PACKET_HEADER_SIZE + len + 1;

    JDConfigurationPacket* cfg = (JDConfigurationPacket *)malloc(JD_CONTROL_CONFIGURATION_SERVICE_PACKET_HEADER_SIZE + len + 1); // add one for the size byte

    cfg->request_type = JD_CONTROL_CONFIGURATION_SERVICE_REQUEST_TYPE_NAME;
    cfg->device_address = device_address;

    *cfg->data = len;

    memcpy(cfg->data + 1, newName.toCharArray(), len);

    send((uint8_t *)cfg, size);

    free(cfg);

    return DEVICE_OK;
}

int JDConfigurationService::triggerRemoteIdentification(uint8_t device_address)
{
    if (device_address == 0)
        return DEVICE_INVALID_PARAMETER;

    JDConfigurationPacket cfg;

    cfg.device_address = device_address;
    cfg.request_type = JD_CONTROL_CONFIGURATION_SERVICE_REQUEST_TYPE_IDENTIFY;

    send((uint8_t *)&cfg, JD_CONTROL_CONFIGURATION_SERVICE_PACKET_HEADER_SIZE);

    return DEVICE_OK;
}
#include "JDNamingService.h"
#include "JACDAC.h"

using namespace codal;

int JDNamingService::send(uint8_t* buf, int len)
{
    if (JACDAC::instance)
        return JACDAC::instance->bus.send(buf, len, this->service_number, NULL);

    return DEVICE_NO_RESOURCES;
}

JDNamingService::JDNamingService(ManagedString name) : JDService(JD_SERVICE_CLASS_CONTROL_NAMING, ClientService), name(name)
{
    this->service_number = JD_CONTROL_NAMING_SERVICE_NUMBER;
}

int JDNamingService::handlePacket(JDPacket* p)
{
    if (!this->device)
        return DEVICE_OK;

    JDNamingServicePacket* pkt = (JDNamingServicePacket *)p->data;

    if (pkt->device_address == this->device->device_address && pkt->request_type == JD_CONTROL_NAMING_SERVICE_REQUEST_TYPE_REQ)
    {
        int len = *pkt->name;
        name = ManagedString((char *)pkt->name + 1, len);
    }

    return DEVICE_OK;
}

int JDNamingService::setRemoteDeviceName(uint8_t device_address, ManagedString newName)
{
    int len = newName.length();

    if (len > JD_SERIAL_MAX_PAYLOAD_SIZE)
        return DEVICE_INVALID_PARAMETER;

    int size = JD_CONTROL_NAMING_SERVICE_PACKET_HEADER_SIZE + len + 1;

    JDNamingServicePacket* pkt = (JDNamingServicePacket *)malloc(JD_CONTROL_NAMING_SERVICE_PACKET_HEADER_SIZE + len + 1); // add one for the size byte

    pkt->request_type = JD_CONTROL_NAMING_SERVICE_REQUEST_TYPE_REQ;
    pkt->device_address = device_address;

    *pkt->name = len;

    memcpy(pkt->name + 1, newName.toCharArray(), len);

    send((uint8_t *)pkt, size);

    free(pkt);

    return DEVICE_OK;
}

int JDNamingService::setName(ManagedString name)
{
    this->name = name;
    return DEVICE_OK;
}

ManagedString JDNamingService::getName()
{
    return this->name;
}
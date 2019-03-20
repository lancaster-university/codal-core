#include "JDService.h"
#include "ManagedString.h"

#ifndef JD_NAMING_SERVICE_H
#define JD_NAMING_SERVICE_H

#define JD_CONTROL_NAMING_SERVICE_NUMBER                    2

#define JD_CONTROL_NAMING_SERVICE_REQUEST_TYPE_REQ          1
#define JD_CONTROL_NAMING_SERVICE_PACKET_HEADER_SIZE        4

struct JDNamingServicePacket
{
    uint16_t device_address;
    uint16_t request_type;
    uint8_t* name;
};

namespace codal
{
    class JDNamingService : public JDService
    {
        ManagedString name;

        int send(uint8_t* buf, int len) override;

        public:
        JDNamingService(ManagedString name);

        virtual int handlePacket(JDPacket* p) override;

        int setRemoteDeviceName(uint8_t device_address, ManagedString newName);

        int setName(ManagedString name);

        ManagedString getName();
    };
}

#endif
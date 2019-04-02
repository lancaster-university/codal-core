#include "JDService.h"
#include "ManagedString.h"

#ifndef JD_NAMING_SERVICE_H
#define JD_NAMING_SERVICE_H

#define JD_CONTROL_CONFIGURATION_SERVICE_NUMBER                     1

#define JD_CONTROL_CONFIGURATION_SERVICE_REQUEST_TYPE_NAME          1
#define JD_CONTROL_CONFIGURATION_SERVICE_REQUEST_TYPE_INDICATE      2

#define JD_CONTROL_CONFIGURATION_SERVICE_PACKET_HEADER_SIZE         2

#define JD_CONTROL_CONFIGURATION_EVT_NAME                           1
#define JD_CONTROL_CONFIGURATION_EVT_INDICATE                       2

struct JDConfigurationPacket
{
    uint8_t device_address;
    uint8_t request_type;
    uint8_t* data;
}__attribute((__packed__));

namespace codal
{
    class JDConfigurationService : public JDService
    {
        int send(uint8_t* buf, int len) override;

        public:
        JDConfigurationService(uint16_t id = DEVICE_ID_JACDAC_CONFIGURATION_SERVICE);

        virtual int handlePacket(JDPacket* p) override;

        int setRemoteDeviceName(uint8_t device_address, ManagedString newName);

        int triggerRemoteIndication(uint8_t device_address);
    };
}

#endif
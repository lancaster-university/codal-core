#ifndef JD_PACKET_LOGGER_H
#define JD_PACKET_LOGGER_H

#include "JACDAC.h"
#include "Event.h"

namespace codal
{

    struct JDSnifferServiceMap
    {
        uint8_t device_address;
        uint32_t services[JD_DEVICE_MAX_HOST_SERVICES];
    };

    class JDPacketSniffer: public JDService
    {
        JDDeviceManager deviceManager;

        void timerCallback(Event);

        public:
        JDPacketSniffer();

        virtual int handlePacket(JDPacket* p) override;

        JDDevice* getDeviceList();

        void logDevices();
    };
}

#endif
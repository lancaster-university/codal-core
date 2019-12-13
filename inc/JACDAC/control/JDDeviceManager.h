#ifndef CODAL_JD_DEVICE_MANAGER_H
#define CODAL_JD_DEVICE_MANAGER_H

#include "CodalConfig.h"
#include "ErrorNo.h"

#define JD_DEVICE_FLAGS_NACK                            0x08
#define JD_DEVICE_FLAGS_HAS_NAME                        0x04
#define JD_DEVICE_FLAGS_PROPOSING                       0x02
#define JD_DEVICE_FLAGS_REJECT                          0x01

#define JD_DEVICE_MAX_HOST_SERVICES                     16

#ifndef JD_DEVICE_DEFAULT_COMMUNICATION_RATE
#define JD_DEVICE_DEFAULT_COMMUNICATION_RATE            JD_SERIAL_BAUD_1M
#endif

struct JDDevice
{
    uint64_t device_identifier;
    uint16_t device_flags;
    uint16_t rolling_counter;
    JDDevice* next;
    uint8_t* name;
};

namespace codal
{
    class JDControlPacket;

    /**
     * This class represents the logic service, which is consistent across all JACDAC devices.
     *
     * It handles addressing and the routing of control packets from the bus to their respective services.
     **/
    class JDDeviceManager
    {
        JDDevice* devices;

        int initialiseDevice(JDDevice* remoteDevice, uint64_t device_identifier, JDControlPacket* controlPacket);

        public:

        JDDevice* getDeviceList();

        JDDevice* getDevice(uint64_t device_identifier);

        JDDevice* addDevice(uint64_t device_identifier, JDControlPacket* controlPacket);

        int updateDevice(JDDevice* remoteDevice, uint64_t device_identifier, JDControlPacket* controlPacket);

        int removeDevice(JDDevice* device);

        JDDeviceManager();
    };
}

#endif
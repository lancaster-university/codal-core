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
    uint64_t udid;
    uint8_t device_flags;
    uint8_t device_address;
    uint8_t communication_rate;
    uint8_t rolling_counter;
    uint16_t servicemap_bitmsk;
    uint8_t broadcast_servicemap[JD_DEVICE_MAX_HOST_SERVICES / 2]; // use to map remote broadcast services to local broadcast services.
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

        int initialiseDevice(JDDevice* remoteDevice, JDControlPacket* controlPacket, uint8_t communicationRate);

        public:

        JDDevice* getDevice();

        JDDevice* getDevice(uint8_t device_address);

        JDDevice* getDevice(uint8_t device_address, uint64_t udid);

        JDDevice* addDevice(JDControlPacket* controlPacket, uint8_t communicationRate);

        int updateDevice(JDDevice* remoteDevice, JDControlPacket* controlPacket, uint8_t communicationRate);

        int removeDevice(JDDevice* device);

        JDDeviceManager();
    };
}

#endif
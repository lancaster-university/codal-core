#ifndef JD_ACCELEROMETER_DRIVER_H
#define JD_ACCELEROMETER_DRIVER_H

#include "JDControlLayer.h"
#include "Accelerometer.h"

#define JD_ACCEL_EVT_SEND_DATA      1

namespace codal
{

    struct AccelerometerPacket
    {
        uint8_t packet_type;
        int16_t x;
        int16_t y;
        int16_t z;
    }__attribute__((packed));

    struct AccelerometerGesturePacket
    {
        uint8_t packet_type;
        int16_t event_value;
    }__attribute__((packed));

    class JDAccelerometerService : public JDService
    {
        Accelerometer* accelerometer;
        Sample3D latest;

        void sendData(Event);
        void forwardEvent(Event evt);

        public:
        JDAccelerometerService(Accelerometer& accel);
        JDAccelerometerService();

        int getX();
        int getY();
        int getZ();

        virtual int handleControlPacket(JDControlPacket* cp);

        virtual int handlePacket(JDPacket* p);
    };
}

#endif
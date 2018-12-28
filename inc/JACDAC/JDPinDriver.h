#ifndef JD_PIN_DRIVER_H
#define JD_PIN_DRIVER_H

#include "JDProtocol.h"
#include "Pin.h"

namespace codal
{
    enum Mode
    {
        SetDigital,
        SetAnalog,
        SetServo
    };

    struct PinPacket
    {
        uint8_t mode;
        uint32_t value;
    };

    class JDPinDriver : public JDDriver
    {
        Pin* pin;

        int sendPacket(Mode m, uint32_t value);

        public:
        JDPinDriver(Pin& p);
        JDPinDriver();

        int setAnalogValue(uint32_t value);
        int setDigitalValue(uint32_t value);
        int setServoValue(uint32_t value);

        virtual int handleControlPacket(JDControlPacket* cp);

        virtual int handlePacket(JDPkt* p);
    };
}

#endif
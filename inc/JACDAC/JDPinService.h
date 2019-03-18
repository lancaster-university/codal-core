#ifndef JD_PIN_SERVICE_H
#define JD_PIN_SERVICE_H

#include "JDControlLayer.h"
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

    class JDPinService : public JDService
    {
        Pin* pin;

        int sendPacket(Mode m, uint32_t value);

        public:
        JDPinService(Pin& p);
        JDPinService();

        int setAnalogValue(uint32_t value);
        int setDigitalValue(uint32_t value);
        int setServoValue(uint32_t value);

        virtual int handlePacket(JDPacket* p);
    };
}

#endif
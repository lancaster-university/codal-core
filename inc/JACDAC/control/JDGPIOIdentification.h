#include "CodalConfig.h"
#include "JDConfigurationService.h"
#include "Pin.h"

#ifndef JD_GPIO_IDENTIFICATION_H
#define JD_GPIO_IDENTIFICATION_H

namespace codal
{
    class JDGPIOIdentification
    {
        Pin& gpio;
        bool identifying;

        public:
        JDGPIOIdentification(Pin& gpio);
        void identify(Event);
    };
}

#endif
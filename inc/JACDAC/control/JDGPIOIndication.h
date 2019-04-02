#include "CodalConfig.h"
#include "JDConfigurationService.h"
#include "Pin.h"

#define JD_DEFAULT_INDICATION_TIME      5       // time in seconds

namespace codal
{
    class JDGPIOIndication
    {
        Pin& gpio;

        public:
        JDGPIOIndication(Pin& gpio);
        void indicate(Event);
    };
}
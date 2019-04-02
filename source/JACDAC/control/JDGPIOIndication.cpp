#include "JDGPIOIndication.h"
#include "MessageBus.h"

using namespace codal;

JDGPIOIndication::JDGPIOIndication(Pin& gpio) : gpio(gpio)
{
    if (EventModel::defaultEventBus)
        EventModel::defaultEventBus->listen(DEVICE_ID_JACDAC_CONFIGURATION_SERVICE, JD_CONTROL_CONFIGURATION_EVT_INDICATE, this, &JDGPIOIndication::indicate);
}

void JDGPIOIndication::indicate(Event)
{
    int state = 0;
    for (int i = 0; i < JD_DEFAULT_INDICATION_TIME * 10; i++)
    {
        gpio.setDigitalValue(state = !state);
        fiber_sleep(100);
    }
}
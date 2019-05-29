#include "JDGPIOIdentification.h"
#include "MessageBus.h"

using namespace codal;

JDGPIOIdentification::JDGPIOIdentification(Pin& gpio) : gpio(gpio)
{
    this->identifying = false;

    if (EventModel::defaultEventBus)
        EventModel::defaultEventBus->listen(DEVICE_ID_JACDAC_CONFIGURATION_SERVICE, JD_CONTROL_CONFIGURATION_EVT_IDENTIFY, this, &JDGPIOIdentification::identify);
}

void JDGPIOIdentification::identify(Event)
{
    if (this->identifying)
        return;

    this->identifying = true;

    int state = 0;
    for (int i = 0; i < JD_DEFAULT_INDICATION_TIME * 10; i++)
    {
        gpio.setDigitalValue(state = !state);
        fiber_sleep(100);
    }

    this->identifying = false;
}
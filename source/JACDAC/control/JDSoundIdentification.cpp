#include "JDSoundIdentification.h"
#include "MessageBus.h"

using namespace codal;

JDSoundIdentification::JDSoundIdentification(Synthesizer& synth) : synth(synth)
{
    this->identifying = false;

    if (EventModel::defaultEventBus)
        EventModel::defaultEventBus->listen(DEVICE_ID_JACDAC_CONFIGURATION_SERVICE, JD_CONTROL_CONFIGURATION_EVT_IDENTIFY, this, &JDSoundIdentification::identify);
}

void JDSoundIdentification::identify(Event)
{
    if (this->identifying)
        return;

    this->identifying = true;

    //                     C4       E4      G4      C5
    float frequencies[5] = {261.63, 329.63, 392.00, 532.25};

    int state = 0;
    for (int j = 0; j < JD_DEFAULT_INDICATION_TIME; j++)
        for (int i = 0; i < 5; i++)
        {
            synth.setFrequency(frequencies[i],100);
            fiber_sleep(100);
        }

    synth.setFrequency(0);

    this->identifying = false;
}
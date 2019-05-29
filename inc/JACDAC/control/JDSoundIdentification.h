#include "CodalConfig.h"
#include "JDConfigurationService.h"
#include "Pin.h"
#include "Synthesizer.h"

#ifndef JD_SOUND_IDENTIFICATION_H
#define JD_SOUND_IDENTIFICATION_H

namespace codal
{
    class JDSoundIdentification
    {
        Synthesizer& synth;
        bool identifying;

        public:
        JDSoundIdentification(Synthesizer& synth);
        void identify(Event);
    };
}

#endif
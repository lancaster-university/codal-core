#include "ManagedBuffer.h"
#include "DataStream.h"
#include "EffectFilter.h"

#ifndef LOW_PASS_FILTER_H
#define LOW_PASS_FILTER_H

namespace codal
{
    class LowPassFilter : public EffectFilter
    {
        private:

        float lpf_value;
        float lpf_beta;

        public:

        LowPassFilter( DataSource &source, float beta = 0.003f, bool deepCopy = true);
        ~LowPassFilter();

        /**
        * Apply a simple low pass filter on the give buffer of data.
        * Y(n) = (1-ß)*Y(n-1) + (ß*X(n))) = Y(n-1) - (ß*(Y(n-1)-X(n)));
        * 
        * @param inputBuffer the buffer containing data to process.
        * @param outputBuffer the buffer in which to store the filtered data. n.b. MAY be the same memory as the input buffer.
        * @param format the format of the data (word size and signed/unsigned representation)
        */
        virtual void applyEffect(ManagedBuffer inputBuffer, ManagedBuffer outputBuffer, int format) override;

        void setBeta( float beta );
    };
}

#endif
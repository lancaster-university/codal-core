#include "ManagedBuffer.h"
#include "DataStream.h"
#include "StreamNormalizer.h"

#ifndef EFFECT_FILTER_H
#define EFFECT_FILTER_H

namespace codal
{
    class EffectFilter : public DataSource, public DataSink
    {
        protected:

        DataSink *downStream;
        DataSource &upStream;
        bool deepCopy;

        public:

        EffectFilter(DataSource &source, bool deepCopy = true);
        ~EffectFilter();

        virtual ManagedBuffer pull() override;
        virtual int pullRequest() override;
        virtual void connect( DataSink &sink ) override;
        virtual bool isConnected() override;
        virtual void disconnect() override;
        virtual int getFormat() override;
        virtual int setFormat( int format ) override;

        virtual float getSampleRate() override;
        virtual float requestSampleRate(float sampleRate) override;
        
        /**
        * Defines if this filter should perform a deep copy of incoming data, or update data in place.
        *
        * @param deepCopy Set to true to copy incoming data into a freshly allocated buffer, or false to change data in place.
        */
        void setDeepCopy(bool deepCopy);

        /**
        * Default effect - a simple pass through filter. Override this method in subclasses to create specialist effects/filters.
        * 
        * @param inputBuffer the buffer containing data to process.
        * @param outputBuffer the buffer in which to store the filtered data. n.b. MAY be the same memory as the input buffer.
        * @param format the format of the data (word size and signed/unsigned representation)
        */
        virtual void applyEffect(ManagedBuffer inputBuffer, ManagedBuffer outputBuffer, int format);
    };
}

#endif
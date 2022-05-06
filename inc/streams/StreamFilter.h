#include "ManagedBuffer.h"
#include "DataStream.h"

#ifndef FILTER_STREAM_H
#define FILTER_STREAM_H

namespace codal
{
    class StreamFilter : public DataSource, public DataSink
    {
        private:

        float lpf_value;

        DataSink *downStream;
        DataSource &upStream;

        public:

        // Just get/set these directly, without the pure function proxy
        bool  lpf_enabled;
        float lpf_beta;

        bool  hpf_enabled;
        float hpf_alpha;

        bool avg_enabled;
        float avg_value;

        StreamFilter( DataSource &source );
        ~StreamFilter();

        virtual ManagedBuffer pull();
        virtual int pullRequest();
    	virtual void connect( DataSink &sink );
        virtual void disconnect();
        virtual int getFormat();
        virtual int setFormat( int format );

        void reset();
        void setLowPassBeta( float beta );
        void enableFunction( int function );

    };
}

#endif
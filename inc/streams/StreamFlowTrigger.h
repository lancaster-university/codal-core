#include "ManagedBuffer.h"
#include "DataStream.h"

#ifndef STREAM_FLOW_TRIGGER_H
#define STREAM_FLOW_TRIGGER_H

#define TRIGGER_PULL    1
#define TRIGGER_REQUEST 2

namespace codal {

    class StreamFlowTrigger : public DataSource, public DataSink {
        private:

        DataSink *downStream;
        DataSource &upStream;

        void (*eventHandler)(int);

        public:

        StreamFlowTrigger( DataSource &source );
        ~StreamFlowTrigger();

        void setDataHandler( void (*handler)(int) );

        virtual ManagedBuffer pull();
        virtual int pullRequest();
    	virtual void connect( DataSink &sink );
        virtual void disconnect();
        virtual int getFormat();
        virtual int setFormat( int format );
    };
}

#endif
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

        virtual ManagedBuffer pull() override;
        virtual int pullRequest() override;
    	virtual void connect( DataSink &sink ) override;
        virtual bool isConnected() override;
        virtual void disconnect() override;
        virtual int getFormat() override;
        virtual int setFormat( int format ) override;
    };
}

#endif
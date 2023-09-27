#ifndef FIFO_STREAM_H
#define FIFO_STREAM_H

#include "ManagedBuffer.h"
#include "DataStream.h"


#define FIFO_MAXIMUM_BUFFERS 256

namespace codal {

    class FIFOStream : public DataSource, public DataSink
    {
        private:

        ManagedBuffer buffer[FIFO_MAXIMUM_BUFFERS];
        int bufferCount;
        int bufferLength;

        bool allowInput;
        bool allowOutput;

        DataSink *downStream;
        DataSource &upStream;

        public:

        FIFOStream( DataSource &source );
        ~FIFOStream();

        virtual ManagedBuffer pull() override;
        virtual int pullRequest() override;
        virtual void connect( DataSink &sink ) override;
        virtual bool isConnected() override;
        virtual void disconnect() override;
        virtual int getFormat() override;
        virtual int setFormat( int format ) override;
        int length();
        void dumpState();

        bool canPull();
        bool isFull();

        void setInputEnable( bool state );
        void setOutputEnable( bool state );


    };

}

#endif
#ifndef FIFO_STREAM_H
#define FIFO_STREAM_H

#include "ManagedBuffer.h"
#include "DataStream.h"


#define FIFO_MAXIMUM_BUFFERS 256

namespace codal {

    class FIFOStream : public DataSourceSink
    {
        private:

        ManagedBuffer buffer[FIFO_MAXIMUM_BUFFERS];
        int bufferCount;
        int bufferLength;

        bool allowInput;
        bool allowOutput;

        public:

        FIFOStream( DataSource &source );
        ~FIFOStream();

        virtual ManagedBuffer pull();
        virtual int pullRequest();
        int length();
        void dumpState();

        bool canPull();
        bool isFull();

        void setInputEnable( bool state );
        void setOutputEnable( bool state );


    };

}

#endif
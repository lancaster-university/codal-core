/*
The MIT License (MIT)

Copyright (c) 2017 Lancaster University.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#ifndef CODAL_DATA_STREAM_H
#define CODAL_DATA_STREAM_H

#include "ManagedBuffer.h"
#include "MessageBus.h"
#include "CodalConfig.h"

#define DATASTREAM_MAXIMUM_BUFFERS      1

// Define valid data representation formats supplied by a DataSource.
// n.b. MUST remain in strict monotically increasing order of sample size.
#define DATASTREAM_FORMAT_UNKNOWN           0 
#define DATASTREAM_FORMAT_8BIT_UNSIGNED     1
#define DATASTREAM_FORMAT_8BIT_SIGNED       2
#define DATASTREAM_FORMAT_16BIT_UNSIGNED    3
#define DATASTREAM_FORMAT_16BIT_SIGNED      4
#define DATASTREAM_FORMAT_24BIT_UNSIGNED    5
#define DATASTREAM_FORMAT_24BIT_SIGNED      6
#define DATASTREAM_FORMAT_32BIT_UNSIGNED    7
#define DATASTREAM_FORMAT_32BIT_SIGNED      8

#define DATASTREAM_FORMAT_BYTES_PER_SAMPLE(x) ((x+1)/2)

#define DATASTREAM_SAMPLE_RATE_UNKNOWN      0.0f

namespace codal
{
    /**
     * Interface definition for a DataSource.
     */
    class DataSink
    {
    	public:
            virtual int pullRequest();
    };

    /**
    * Interface definition for a DataSource.
    */
    class DataSource
    {
    	public:
            virtual ManagedBuffer pull();
            virtual void connect(DataSink &sink);
            virtual bool isConnected() { return false; }
            virtual void disconnect();
            virtual int getFormat();
            virtual int setFormat(int format);
            virtual float getSampleRate();
    };

    /**
      * Class definition for DataStream.
      * A Datastream holds a number of ManagedBuffer references, provides basic flow control through a push/pull mechanism
      * and byte level access to the datastream, even if it spans different buffers.
      */
    class DataStream : public DataSource, public DataSink
    {
        uint16_t pullRequestEventCode;
        uint16_t flowEventCode;
        ManagedBuffer nextBuffer;
        bool hasPending;
        bool isBlocking;
        unsigned int missedBuffers;
        int downstreamReturn;

        DataSink *downStream;
        DataSource *upStream;

        public:

            /**
             * Default Constructor.
             * Creates an empty DataStream.
             *
             * @param upstream the component that will normally feed this datastream with data.
             */
            DataStream(DataSource &upstream);

            /**
             * Destructor.
             * Removes all resources held by the instance.
             */
            ~DataStream();

            /**
             * Controls if this component should emit flow state events.
             * 
             * @warning Should not be called mutliple times with `id == 0`, as it will spuriously reallocate event IDs
             * 
             * @param id If zero, this will auto-allocate a new event ID
             * @return uint16_t The new event ID for this DataStream
             */
            uint16_t emitFlowEvents( uint16_t id = 0 );

            /**
             * Determines if any of the data currently flowing through this stream is held in non-volatile (FLASH) memory.
             * @return true if one or more of the ManagedBuffers in this stream reside in FLASH memory, false otherwise.
             */
            bool isReadOnly();

            /**
             * Attempts to determine if another component downstream of this one is _actually_ pulling data, and thus, data
             * is flowing.
             * 
             * @return true If there is a count-match between `pullRequest` and `pull` calls.
             * @return false If `pullRequest` calls are not currently being matched by `pull` calls.
             */
            bool isFlowing();

            /**
             * Define a downstream component for data stream.
             *
             * @sink The component that data will be delivered to, when it is available
             */
            virtual void connect(DataSink &sink) override;

            /**
             * Determines if this source is connected to a downstream component
             * 
             * @return true If a downstream is connected
             * @return false If a downstream is not connected
             */
            virtual bool isConnected();

            /**
             * Define a downstream component for data stream.
             *
             * @sink The component that data will be delivered to, when it is available
             */
            virtual void disconnect() override;

            /**
             *  Determine the data format of the buffers streamed out of this component.
             */
            virtual int getFormat() override;

            /**
             * Determines if this stream acts in a synchronous, blocking mode or asynchronous mode. In blocking mode, writes to a full buffer
             * will result int he calling fiber being blocked until space is available. Downstream DataSinks will also attempt to process data
             * immediately as it becomes available. In non-blocking asynchronpus mode, writes to a full buffer are dropped and downstream Datasinks will
             * be processed in a new fiber.
             */
            void setBlocking(bool isBlocking);

            /**
             * Determines if a buffer of the given size can be added to the buffer.
             *
             * @param size The number of bytes to add to the buffer.
             * @return true if there is space for "size" bytes in the buffer. false otherwise.
             */
            bool canPull(int size = 0);

            /**
             * Provide the next available ManagedBuffer to our downstream caller, if available.
             */
            virtual ManagedBuffer pull();

            /**
             * Deliver the next available ManagedBuffer to our downstream caller.
             */
            virtual int pullRequest();

            /**
             * Query the stream for its current sample rate.
             * 
             * If the current object is unable to determine this itself, it will pass the call upstream until it reaches a component can respond.
             * 
             * @warning The sample rate for a stream may change during its lifetime. If a component is sensitive to this, it should periodically check.
             * 
             * @return float The current sample rate for this stream, or `DATASTREAM_SAMPLE_RATE_UNKNOWN` if none is found.
             */
            virtual float getSampleRate() override;

        private:
            /**
             * Issue a deferred pull request to our downstream component, if one has been registered.
             */
            void onDeferredPullRequest(Event);

    };
}

#endif

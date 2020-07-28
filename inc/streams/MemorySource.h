#include "CodalConfig.h"
#include "DataStream.h"

#ifndef MEMORY_SOURCE_H
#define MEMORY_SOURCE_H

#define MEMORY_SOURCE_DEFAULT_MAX_BUFFER        256

/**
 * A simple buffer class for streaming bytes from memory across the Stream APIs.
 */
namespace codal
{
    class MemorySource : public DataSource
    {
        private:
        int             outputFormat;           // The format to output in. By default, this is the same as the input.
        int             outputBufferSize;       // The maximum size of an output buffer.
        int             bytesSent;              // The number of bytes sent in the current buffer
        ManagedBuffer   buffer;                 // The output buffer being filled

        public:
        DataStream output;

        /**
         * Default Constructor.
         */
        MemorySource();

        /**
         * Provide the next available ManagedBuffer to our downstream caller, if available.
         */
        virtual ManagedBuffer pull();

        /**
         *  Determine the data format of the buffers streamed out of this component.
         */
        virtual int getFormat();

        /**
         * Defines the data format of the buffers streamed out of this component.
         * @param format valid values include:
         * 
         * DATASTREAM_FORMAT_8BIT_UNSIGNED
         * DATASTREAM_FORMAT_8BIT_SIGNED
         * DATASTREAM_FORMAT_16BIT_UNSIGNED
         * DATASTREAM_FORMAT_16BIT_SIGNED
         * DATASTREAM_FORMAT_24BIT_UNSIGNED
         * DATASTREAM_FORMAT_24BIT_SIGNED
         * DATASTREAM_FORMAT_32BIT_UNSIGNED
         * DATASTREAM_FORMAT_32BIT_SIGNED
         */
        virtual int setFormat(int format);

        /**
         *  Determine the maximum size of the buffers streamed out of this component.
         *  @return The maximum size of this component's output buffers, in bytes.
         */
        int getBufferSize();

        /**
         *  Defines the maximum size of the buffers streamed out of this component.
         *  @param size the size of this component's output buffers, in bytes.
         */
        int setBufferSize(int size);

        /**
         * Perform a blocking playout of the data buffer. Returns when all the data has been queued.
         * @param data pointer to memory location to playout
         * @param length number of samples in the buffer. Assumes a sample size as defined by setFormat().
         * @param loop if repeat playback of buffer when completed the given number of times.  Defaults to one. Set to a negative number to loop forever.
         */
        void play(const void *data, int length, int loop = 1);

        /**
         * Perform a blocking playout of the data buffer. Returns when all the data has been queued.
         * @param b the buffer to playout
         * @param loop if repeat playback of buffer when completed the given number of times. Defaults to one. Set to a negative number to loop forever.
         */
        void play(ManagedBuffer b, int loop = 1);
    };
}
#endif
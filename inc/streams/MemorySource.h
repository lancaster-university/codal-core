#include "CodalConfig.h"
#include "DataStream.h"

#ifndef MEMORY_SOURCE_H
#define MEMORY_SOURCE_H

#define MEMORY_SOURCE_MAX_BUFFER        256

/**
 * A simple buffer class for streaming bytes from memory across the Stream APIs.
 */
namespace codal
{
    class MemorySource : public DataSource
    {
        private:
        int bytesSent;
        ManagedBuffer buffer;
        bool loop;

        int maximumValue;
        int scalar;

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
         * Perform a blocking playout of the data buffer. Returns when all the data has been queued.
         * (or never if loop is set).
         * @param data pointer to memory location to playout
         * @param length number of bytes to stream
         * @param loop if repeat playback of buffer when completed the given number of times
         * @param expand repeat each input sample the given number of times in the output stream.
         */
        void play(const uint8_t *data, int length, int loop = 1, int expand = 1);

        /**
         * Perform a 16 bit blocking playout of a 16 bit data buffer. 
         * Returns when all the data has been queued.
         * @param data pointer to memory location to playout.
         * @param length number of 16 bit words to stream.
         * @param loop  repeat playback of buffer when completed the given number of times. Set to zero to repeat forever.
         * @param expand repeat each input sample the given number of times in the output stream.
         */
        void play(const uint16_t *data, int length, int loop = 1, int expand = 1);

        /**
         * Define the upper bound for samples in the 16 bit stream
         */
        void setMaximumSampleValue(int maximum);

    };
}
#endif
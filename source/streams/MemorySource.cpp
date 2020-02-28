#include "MemorySource.h"
#include "CodalDmesg.h"

using namespace codal;

/**
 * Default Constructor.
 *
 * @param data pointer to memory location to playout
 * @param length number of bytes to stream
 */
MemorySource::MemorySource() : output(*this)
{
    this->bytesSent = 0;
    this->maximumValue = 256;
    this->scalar = 1;
} 

/**
 * Define the upper bound for samples in the 16 bit stream
 */
void MemorySource::setMaximumSampleValue(int maximum)
{
    maximumValue = maximum;
    scalar = (maximumValue << 8) / 256;
}

/**
 * Provide the next available ManagedBuffer to our downstream caller, if available.
 */
ManagedBuffer MemorySource::pull()
{
    return buffer;
} 

/**
 * Perform a 16 bit blocking playout of the 8 bit data buffer. 
 * Samples are upscaled to 16 bit. Returns when all the data has been queued.
 * @param data pointer to memory location to playout.
 * @param length number of bytes to stream.
 * @param loop  repeat playback of buffer when completed the given number of times. Set to zero to repeat forever.
 * @param expand repeat each input sample the given number of times in the output stream.
 */
void MemorySource::play(const uint8_t *data, int length, int loop, int expand)
{
    int sample;
    uint16_t *out;

    do
    {
        while (bytesSent < length)
        {
            int size = min(length - bytesSent, MEMORY_SOURCE_MAX_BUFFER);
            buffer = ManagedBuffer(size*expand*2);
            out = (uint16_t *) &buffer[0];

            for (int i=0; i<size; i++)
            {
                sample = data[bytesSent];
                bytesSent++;

                if (sample == 255)
                    sample = maximumValue;
                else
                    sample = (sample * scalar) >> 8;

                for (int s=0; s < expand; s++)
                {
                    *out = sample; 
                    out++;
                }
            }

            output.pullRequest();
        }

        bytesSent = 0;
        if (loop > 0)
            loop--;

    } while (loop);

} 

/**
 * Perform a 16 bit blocking playout of a 16 bit data buffer. 
 * Returns when all the data has been queued.
 * @param data pointer to memory location to playout.
 * @param length number of 16 bit words to stream.
 * @param loop  repeat playback of buffer when completed the given number of times. Set to zero to repeat forever.
 * @param expand repeat each input sample the given number of times in the output stream.
 */
void MemorySource::play(const uint16_t *data, int length, int loop, int expand)
{
    int sample;
    uint16_t *out;

    do
    {
        while (bytesSent < length)
        {
            int size = min(length - bytesSent, MEMORY_SOURCE_MAX_BUFFER);

            buffer = ManagedBuffer(size*expand*2);
            out = (uint16_t *) &buffer[0];

            for (int i=0; i<size; i++)
            {
                sample = data[bytesSent];
                bytesSent++;

                sample = min(sample, maximumValue);

                for (int s=0; s < expand; s++)
                {
                    *out = sample; 
                    out++;
                }
            }

            output.pullRequest();
        }
        
        bytesSent = 0;
        if (loop > 0)
            loop--;

    } while (loop);

} 

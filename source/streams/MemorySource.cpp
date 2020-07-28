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
    this->setFormat(DATASTREAM_FORMAT_8BIT_UNSIGNED);
    this->setBufferSize(MEMORY_SOURCE_DEFAULT_MAX_BUFFER);
} 

/**
 *  Determine the data format of the buffers streamed out of this component.
 */
int
MemorySource::getFormat()
{
    return outputFormat;
}

/**
 * Defines the data format of the buffers streamed out of this component.
 * @param format the format to use, one of
 */
int
MemorySource::setFormat(int format)
{
    outputFormat = format;
    return DEVICE_OK;
}

/**
 *  Determine the maximum size of the buffers streamed out of this component.
 *  @return The maximum size of this component's output buffers, in bytes.
 */
int
MemorySource::getBufferSize()
{
    return outputBufferSize;
}

/**
 *  Defines the maximum size of the buffers streamed out of this component.
 *  @param size the size of this component's output buffers, in bytes.
 */
int
MemorySource::setBufferSize(int size)
{
    outputBufferSize = size;
    return DEVICE_OK;
}

/**
 * Provide the next available ManagedBuffer to our downstream caller, if available.
 */
ManagedBuffer MemorySource::pull()
{
    return buffer;
} 

/**
 * Perform a blocking playout of the data buffer. Returns when all the data has been queued.
 * @param data pointer to memory location to playout
 * @param length number of samples in the buffer. Assumes a sample size as defined by setFormat().
 * @param loop if repeat playback of buffer when completed the given number of times. Set to a negative number to loop forever.
 */
void MemorySource::play(const void *data, int length, int loop)
{
    uint8_t *out;
    uint8_t *in;

    in = (uint8_t *)data;
    length = length * DATASTREAM_FORMAT_BYTES_PER_SAMPLE(outputFormat);
   
    do
    {
        while (bytesSent < length)
        {
            int size = min(length - bytesSent, outputBufferSize);
            buffer = ManagedBuffer(size);
            out = &buffer[0];

            memcpy(out, in, size);
            in += size;
            bytesSent += size;

            output.pullRequest();
        }

        bytesSent = 0;
        if (loop > 0)
            loop--;

    } while (loop);
} 

/**
 * Perform a blocking playout of the data buffer. Returns when all the data has been queued.
 * @param b the buffer to playout
 * @param loop if repeat playback of buffer when completed the given number of times. Set to a negative number to loop forever.
 */
void MemorySource::play(ManagedBuffer b, int loop)
{
    do
    {
        buffer = b;
        output.pullRequest();
 
        if (loop > 0)
            loop--;

    } while (loop);
} 

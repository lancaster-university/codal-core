#include "StreamNormalizer.h"
#include "ErrorNo.h"
#include "CodalDmesg.h"

using namespace codal;

StreamNormalizer::StreamNormalizer(DataSource &source, int gain) : upstream(source), output(*this)
{
    this->gain = gain;
    this->zeroOffset = 0;

    // Register with our upstream component
    source.connect(*this);
}

/**
 * Provide the next available ManagedBuffer to our downstream caller, if available.
 */
ManagedBuffer StreamNormalizer::pull()
{
    return buffer;
}

/**
 * Callback provided when data is ready.
 */
int StreamNormalizer::pullRequest()
{
    int z = 0;
    int minimum = 0;
    int maximum = 0;
    int s;
    int16_t result;

    buffer = upstream.pull();

    int16_t *data = (int16_t *) &buffer[0];
    int samples = buffer.length() / 2;

    for (int i=0; i < samples; i++)
    {
        z += *data;

        s = (int) *data;
        s = s - zeroOffset;
        s = s * gain;
        s = s / 1024;

        result = (int16_t)s;

        if (s < minimum)
            minimum = s;

        if (s > maximum)
            maximum = s;

        *data = result;
        data++;
    }

    z = z / samples;
    zeroOffset = z;
    //DMESG("[Z: %d] [R: %d]", zeroOffset, maximum - minimum);

    output.pullRequest();
    return DEVICE_OK;
}

int
StreamNormalizer::setGain(int gain)
{
    this->gain = gain;
    return DEVICE_OK;
}

int
StreamNormalizer::getGain()
{
    return gain;
}

/**
 * Destructor.
 */
StreamNormalizer::~StreamNormalizer()
{
}

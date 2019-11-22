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

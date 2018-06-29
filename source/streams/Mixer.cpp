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

#include "Mixer.h"
#include "ErrorNo.h"
#include "CodalDmesg.h"

using namespace codal;

Mixer::Mixer()
{
    channels = NULL;
    downStream = NULL;
}

Mixer::~Mixer()
{
    while (channels)
    {
        auto n = channels;
        channels = n->next;
        n->stream->disconnect();
        delete n;
    }
}

MixerChannel *Mixer::addChannel(DataStream &stream)
{
    auto c = new MixerChannel();
    c->stream = &stream;
    c->next = channels;
    c->volume = 1024;
    channels = c;
    stream.connect(*this);
    return c;
}

ManagedBuffer Mixer::pull() {
    if (!channels)
        return ManagedBuffer(512);

    ManagedBuffer sum;
    MixerChannel *next;

    for (auto ch = channels; ch; ch = next) {
        next = ch->next; // save next in case the current channel gets deleted
        int vol = ch->volume;
        ManagedBuffer data = ch->stream->pull();
        if (sum.length() < data.length()) {
            ManagedBuffer newsum(data.length());
            newsum.writeBuffer(0, sum);
            for (int i = sum.length(); i < newsum.length(); i += 2)
                *((uint16_t*)&newsum[i]) = 512;
            sum = newsum;
        }
        auto d = (uint16_t*)&data[0];
        auto s = (uint16_t*)&sum[0];
        auto len = data.length() >> 1;
        while (len--) {
            int v = ((((int)*d - 512) * (int)vol) + (((int)*s - 512) << 10)) >> 10;
            v += 512;
            if (v < 0) v = 0;
            if (v > 1023) v = 1023;
            *s = v;
            d++;
            s++;
        }
    }

    return sum;
}

int Mixer::pullRequest()
{
    // we might call it too much if we have more than one channel, but we
    // assume the downStream is only going to call pull() as much as it needs
    // and not more
    if (downStream)
        downStream->pullRequest();
    return DEVICE_OK;
}

void Mixer::connect(DataSink &sink)
{
    this->downStream = &sink;
}

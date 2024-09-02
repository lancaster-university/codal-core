/*
The MIT License (MIT)

Copyright (c) 2021 Lancaster University.

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

#include "CodalConfig.h"
#include "DataStream.h"

#ifndef STREAM_SPLITTER_H
#define STREAM_SPLITTER_H

#ifndef CONFIG_MAX_CHANNELS
#define CONFIG_MAX_CHANNELS 10
#endif

// 4 steps are usually sufficient to get reasonable quality audio
#ifndef CONFIG_SPLITTER_OVERSAMPLE_STEP
#define CONFIG_SPLITTER_OVERSAMPLE_STEP 16
#endif

/**
 * Splitter events
 */
#define SPLITTER_CHANNEL_CONNECT    1
#define SPLITTER_CHANNEL_DISCONNECT 2
#define SPLITTER_ACTIVATE           3
#define SPLITTER_DEACTIVATE         4
#define SPLITTER_TICK               10

/**
 * Default configuration values
 */

namespace codal {

class StreamSplitter;

class SplitterChannel : public DataSource, public DataSink {
  private:
    StreamSplitter* parent;
    float sampleRate;
    unsigned int inUnderflow;

    ManagedBuffer resample(ManagedBuffer _in, uint8_t* buffer = NULL, int length = -1);

  public:
    int pullAttempts;  // Number of failed pull request attempts
    uint32_t sentBuffers;
    DataSink* output;

    /**
     * @brief Construct a new Splitter Channel object.
     *
     * This should not normally be done manually; StreamSplitter objects will create these
     * on-demand via createChannel()
     *
     * @param parent The StreamSplitter this channel is part of
     * @param output An output DataSink to send data to. Can be NULL for a disconnected channel.
     */
    SplitterChannel(StreamSplitter* parent, DataSink* output);
    virtual ~SplitterChannel();

    virtual int pullRequest();
    uint8_t* pullInto(uint8_t* rawBuffer, int length);
    virtual ManagedBuffer pull();
    virtual void connect(DataSink& sink);
    bool isConnected();
    virtual void disconnect();
    virtual int getFormat();
    virtual int setFormat(int format);
    virtual float getSampleRate();
    virtual float requestSampleRate(float sampleRate);
};

class StreamSplitter : public DataSink, public CodalComponent {
  private:
    ManagedBuffer lastBuffer;  // Buffer being processed
    uint64_t __cycle;

  public:
    bool isActive;                // Track if we need to emit activate/deactivate messages
    int channels;                 // Current number of channels Splitter is serving
    volatile int activeChannels;  // Current number of /active/ channels this Splitter is serving
    DataSource& upstream;         // The upstream component of this Splitter
    SplitterChannel* outputChannels[CONFIG_MAX_CHANNELS];  // Array of SplitterChannels the Splitter is serving

    /**
     * Creates a component that distributes a single upstream datasource to many downstream datasinks
     *
     * @param source a DataSource to receive data from
     */
    StreamSplitter(DataSource& source, uint16_t id = CodalComponent::generateDynamicID());

    /**
     * Callback provided when data is ready.
     */
    virtual int pullRequest();

    virtual ManagedBuffer getBuffer();
    virtual SplitterChannel* createChannel();
    virtual bool destroyChannel(SplitterChannel* channel);
    virtual SplitterChannel* getChannel(DataSink* output);

    /**
     * Destructor.
     */
    virtual ~StreamSplitter();

    friend SplitterChannel;
};
}  // namespace codal

#endif
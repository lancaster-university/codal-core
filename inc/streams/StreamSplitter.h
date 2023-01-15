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

#ifndef CONFIG_BLOCKING_THRESHOLD
#define CONFIG_BLOCKING_THRESHOLD 100
#endif

/**
  * Splitter events
  */
#define SPLITTER_ACTIVATE_CHANNEL    1
#define SPLITTER_DEACTIVATE_CHANNEL  2


/**
 * Default configuration values
 */

namespace codal{

    class StreamSplitter;

    class SplitterChannel : public DataSource, public DataSink {
        private:
            StreamSplitter * parent;
            float sampleRate;
        
        public:
            DataSink * output;

            SplitterChannel( StreamSplitter *parent, DataSink *output );
            ~SplitterChannel();

            virtual int pullRequest();
            virtual ManagedBuffer pull();
            virtual void connect(DataSink &sink);
            virtual void disconnect();
            virtual int getFormat();
            virtual int setFormat(int format);
            virtual float getSampleRate();
            virtual float requestSampleRate(float sampleRate);
    };

    class StreamSplitter : public DataSink, public CodalComponent 
    {
    private:
        ManagedBuffer       lastBuffer;                            // Buffer being processed

    public:    
        int                 numberChannels;                        // Current Number of channels Splitter is serving
        int                 processed;                             // How many downstream components have responded to pull request
        //int                 numberAttempts;                      // Number of failed pull request attempts
        DataSource          &upstream;                             // The upstream component of this Splitter
        SplitterChannel   * outputChannels[CONFIG_MAX_CHANNELS];   // Array of SplitterChannels the Splitter is serving

        /**
          * Creates a component that distributes a single upstream datasource to many downstream datasinks
          *
          * @param source a DataSource to receive data from
          */
        StreamSplitter(DataSource &source, uint16_t id = CodalComponent::generateDynamicID());

        /**
         * Callback provided when data is ready.
         */
        virtual int pullRequest();

        virtual ManagedBuffer getBuffer();
        virtual SplitterChannel * createChannel();
        virtual SplitterChannel * getChannel( DataSink * output );

        /**
         * Destructor.
         */
        ~StreamSplitter();

    };
}

#endif
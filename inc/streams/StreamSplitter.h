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

    class StreamSplitter : public DataSink, public DataSource, public CodalComponent 
    {

    public:    
        int                 numberChannels;                    // Current Number of channels Splitter is serving
        int                 processed;                         // How many downstream components have responded to pull request
        int                 numberAttempts;                    // Number of failed pull request attempts
        DataSource          &upstream;                         // The upstream component of this Splitter
        DataSink            *outputChannels[CONFIG_MAX_CHANNELS];     // Array of Datasinks the Splitter is serving
        ManagedBuffer       lastBuffer;                        // Buffer being processed

        /**
          * Creates a component that distributes a single upstream datasource to many downstream datasinks
          *
          * @param source a DataSource to receive data from
          */
        StreamSplitter(DataSource &source, uint16_t id = DEVICE_ID_SPLITTER);

        /**
         * Callback provided when data is ready.
         */
        virtual int pullRequest();

        /**
         * Provide the next available ManagedBuffer to our downstream caller, if available.
         */
        virtual ManagedBuffer pull();

        /**
         * Register a downstream connection with splitter
         */
        virtual void connect(DataSink &downstream);

        /**
         *  Determine the data format of the buffers streamed out of this component.
         */
        virtual int getFormat();

        /**
         * Defines the data format of the buffers streamed out of this component.
         * @param format the format to use, one of
         */
        virtual int setFormat(int format);

        /**
         * Destructor.
         */
        ~StreamSplitter();

    };
}

#endif
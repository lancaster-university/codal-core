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

#include "CodalConfig.h"
#include "DataStream.h"

#ifndef STREAM_NORMALIZER_H
#define STREAM_NORMALIZER_H

/**
 * Default configuration values
 */

namespace codal{

    class StreamNormalizer : public DataSink, public DataSource
    {
    public:
        int             gain;                   // Gain to apply, in 1024ths of a unit. i.e. a value of 2048 would yield a gain of x2.
        int             zeroOffset;             // unsigned value that is the best effort guess of the zero point of the data source
        DataSource      &upstream;
        DataStream      output;
        ManagedBuffer   buffer;

        /**
          * Creates a component capable of translating one data representation format into another
          *
          * @param source a DataSource to receive data from.
          * @param inputFormat The format describing how the data being received should be interpreted.
          * @param outputFormat The format to convter the input stream into.
          */
        StreamNormalizer(DataSource &source, int gain);

        /**
         * Callback provided when data is ready.
         */
    	virtual int pullRequest();

        /**
         * Provide the next available ManagedBuffer to our downstream caller, if available.
         */
        virtual ManagedBuffer pull();

        int setGain(int gain);

        int getGain();

        /**
         * Destructor.
         */
        ~StreamNormalizer();

    };
}

#endif

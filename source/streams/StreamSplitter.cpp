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
#include "StreamSplitter.h"
#include "StreamNormalizer.h"
#include "ErrorNo.h"
#include "CodalDmesg.h"
#include "Event.h"

using namespace codal;

SplitterChannel::SplitterChannel( StreamSplitter * parent, DataSink * output = NULL )
{
    this->sampleRate = DATASTREAM_SAMPLE_RATE_UNKNOWN;
    this->parent = parent;
    this->output = output;
}

SplitterChannel::~SplitterChannel()
{
    //
}

int SplitterChannel::pullRequest() {
    if( output != NULL )
        return output->pullRequest();
    return -1;
}

ManagedBuffer SplitterChannel::pull()
{
    // Shortcut!
    if( sampleRate == DATASTREAM_SAMPLE_RATE_UNKNOWN )
        return parent->getBuffer();
    
    // Going the long way around - drop any extra samples...
    float inRate = parent->upstream.getSampleRate();
    ManagedBuffer inData = parent->getBuffer();
    int inFmt = parent->upstream.getFormat();
    int bytesPerSample = DATASTREAM_FORMAT_BYTES_PER_SAMPLE( inFmt );
    int inSamples = inData.length() / bytesPerSample;
    int step = (inRate / sampleRate);

    ManagedBuffer outData = ManagedBuffer( (inSamples/step) * bytesPerSample );

    uint8_t *inPtr = &inData[0];
    uint8_t *outPtr = &outData[0];
    for( int i=0; i<inSamples/step; i++ ) {
        int s = StreamNormalizer::readSample[inFmt](inPtr);
        inPtr += bytesPerSample * step;

        StreamNormalizer::writeSample[inFmt](outPtr, s);
        outPtr += bytesPerSample;
    }

    return outData;
}

void SplitterChannel::connect(DataSink &sink)
{
    output = &sink;
}

void SplitterChannel::disconnect()
{
    output = NULL;
}

int SplitterChannel::getFormat()
{
    return parent->upstream.getFormat();
}

int SplitterChannel::setFormat(int format)
{
    return parent->upstream.setFormat( format );
}

float SplitterChannel::getSampleRate()
{
    if( sampleRate != DATASTREAM_SAMPLE_RATE_UNKNOWN )
        return sampleRate;
    return parent->upstream.getSampleRate();
}

float SplitterChannel::requestSampleRate( float sampleRate )
{
    sampleRate = sampleRate;

    // Do we need to request a higher rate upstream?
    if( parent->upstream.getSampleRate() < sampleRate ) {

        // Request it, and if we got less that we expected, report that rate
        if( parent->upstream.requestSampleRate( sampleRate ) < sampleRate )
            return parent->upstream.getSampleRate();
    }

    // Otherwise, report our own rate (we're matching or altering it ourselves)
    return sampleRate;
}





/**
 * Creates a component that distributes a single upstream datasource to many downstream datasinks
 *
 * @param source a DataSource to receive data from
 */
StreamSplitter::StreamSplitter(DataSource &source, uint16_t id) : upstream(source)
{
    this->id = id;
    this->numberChannels = 0;
    // init array to NULL.
    for (int i = 0; i < CONFIG_MAX_CHANNELS; i++)
        outputChannels[i] = NULL;
    
    source.connect(*this);
}

StreamSplitter::~StreamSplitter()
{
    // Nop.
}

ManagedBuffer StreamSplitter::getBuffer()
{
    processed++;
    return lastBuffer;
}

/**
 * Callback provided when data is ready.
 */
int StreamSplitter::pullRequest()
{
    if( processed >= numberChannels )
    {
        processed = 0;
        lastBuffer = upstream.pull();

        // For each downstream channel that exists in array outputChannels - make a pullRequest
        for (int i = 0; i < CONFIG_MAX_CHANNELS; i++)
            if (outputChannels[i] != NULL)
                outputChannels[i]->pullRequest();
    }
    else {
        // Unfortunately we have to drop a buffer, otherwise we might stall the pipeline!
        upstream.pull();
        processed = CONFIG_MAX_CHANNELS + 1;
    }
    
    return DEVICE_OK;
}


SplitterChannel * StreamSplitter::createChannel()
{
    int placed = -1;
    for (int i = 0; i < CONFIG_MAX_CHANNELS; i++)
    {
        // Add downstream as one of the splitters datasinks that will be served
        if (outputChannels[i] == NULL){
            outputChannels[i] = new SplitterChannel( this, NULL );
            placed = i;
            DMESG("%s %d","Channel Added at location ", i);
            break;
        }
    }
    if(placed != -1)
        numberChannels++;

    if(numberChannels > 0){
        //Activate ADC
        Event e( id, SPLITTER_ACTIVATE_CHANNEL );
    }

    if( placed != -1 )
        return outputChannels[placed];
    
    return NULL;
}

SplitterChannel * StreamSplitter::getChannel( DataSink * output ) {
    for( int i=0; i<CONFIG_MAX_CHANNELS; i++ )
    {
        if( outputChannels[i] != NULL )
        {
            if( outputChannels[i]->output == output ) {
                return outputChannels[i];
            }
        }
    }

    return NULL;
}
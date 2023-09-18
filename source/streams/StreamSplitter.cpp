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
#include "Event.h"

using namespace codal;

SplitterChannel::SplitterChannel( StreamSplitter * parent, DataSink * output = NULL )
{
    this->sampleRate = DATASTREAM_SAMPLE_RATE_UNKNOWN;
    this->parent = parent;
    this->output = output;
    this->pullAttempts = 0;
    this->sentBuffers = 0;
}

SplitterChannel::~SplitterChannel()
{
    //
}

int SplitterChannel::pullRequest() {
    this->pullAttempts++;
    if( output != NULL )
        return output->pullRequest();
    return DEVICE_BUSY;
}

ManagedBuffer SplitterChannel::pull()
{
    this->pullAttempts = 0;
    this->sentBuffers++;
    ManagedBuffer inData = parent->getBuffer();

    // Shortcuts - we can't fabricate samples, so just pass on what we can if we don't know or can't keep up.
    if( sampleRate == DATASTREAM_SAMPLE_RATE_UNKNOWN || sampleRate >= parent->upstream.getSampleRate() )
        return inData;
    
    // Going the long way around - drop any extra samples...
    float inRate = parent->upstream.getSampleRate();
    int inFmt = parent->upstream.getFormat();
    int bytesPerSample = DATASTREAM_FORMAT_BYTES_PER_SAMPLE( inFmt );
    int inSamples = inData.length() / bytesPerSample;
    int step = (inRate / sampleRate);

    ManagedBuffer outData = ManagedBuffer( (inSamples/step) * bytesPerSample );

    uint8_t *inPtr = &inData[0];
    uint8_t *outPtr = &outData[0];
    while( outPtr - &outData[0] < outData.length() )
    {
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
    Event e( parent->id, SPLITTER_CHANNEL_CONNECT );
}

bool SplitterChannel::isConnected()
{
    return this->output != NULL;
}

void SplitterChannel::disconnect()
{
    output = NULL;
    Event e( parent->id, SPLITTER_CHANNEL_DISCONNECT );
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
    this->sampleRate = sampleRate;

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
    this->channels = 0;
    this->activeChannels = 0;
    this->isActive = false;

    // init array to NULL.
    for (int i = 0; i < CONFIG_MAX_CHANNELS; i++)
        outputChannels[i] = NULL;
    
    upstream.connect(*this);

    this->__cycle = 0;
    //this->status |= DEVICE_COMPONENT_STATUS_SYSTEM_TICK;
}

StreamSplitter::~StreamSplitter()
{
    // Nop.
}

ManagedBuffer StreamSplitter::getBuffer()
{
    if( lastBuffer == ManagedBuffer() )
        lastBuffer = upstream.pull();
    
    return lastBuffer;
}

/**
 * Callback provided when data is ready.
 */
int StreamSplitter::pullRequest()
{

    activeChannels = 0;

    // For each downstream channel that exists in array outputChannels - make a pullRequest
    for (int i = 0; i < CONFIG_MAX_CHANNELS; i++)
    {
        if (outputChannels[i] != NULL) {
            if( outputChannels[i]->pullRequest() == DEVICE_OK ) {
                activeChannels++;

                if( !isActive )
                    Event e( id, SPLITTER_ACTIVATE );
                isActive = true;
            }
        }
    }
    
    if( activeChannels == 0 && isActive ) {
        Event e( id, SPLITTER_DEACTIVATE );
        isActive = false;
    }

    lastBuffer = ManagedBuffer();
    return DEVICE_BUSY;
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
            break;
        }
    }
    if(placed != -1) {
        channels++;
        return outputChannels[placed];
    }
    
    return NULL;
}

bool StreamSplitter::destroyChannel( SplitterChannel * channel ) {
    for( int i=0; i<CONFIG_MAX_CHANNELS; i++ ) {
        if( outputChannels[i] == channel ) {
            outputChannels[i] = NULL;
            channels--;
            delete channel;
            return true;
        }
    }

    return false;
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
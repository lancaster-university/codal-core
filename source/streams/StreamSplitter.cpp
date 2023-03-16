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
#include "CodalDmesg.h"

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
    pullAttempts++;
    if( output != NULL )
        return output->pullRequest();
    return -1;
}

ManagedBuffer SplitterChannel::pull()
{
    pullAttempts = 0;
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
    // Prevent repeated events on calling connect multiple times (so consumers can blindly connect!)
    if( output != &sink ) {
        output = &sink;
        parent->activeChannels++; // Notify that we have might _at least_ one sink available
        Event e( parent->id, SPLITTER_CHANNEL_CONNECT );
    }
}

void SplitterChannel::disconnect()
{
    // Prevent repeated events on calling disconnect multiple times (so consumers can blindly disconnect!)
    if( output != NULL ) {
        output = NULL;
        Event e( parent->id, SPLITTER_CHANNEL_DISCONNECT );
    }
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
    // init array to NULL.
    for (int i = 0; i < CONFIG_MAX_CHANNELS; i++)
        outputChannels[i] = NULL;
    
    source.connect(*this);

    this->__cycle = 0;
    //this->status |= DEVICE_COMPONENT_STATUS_SYSTEM_TICK;
}

StreamSplitter::~StreamSplitter()
{
    // Nop.
}

ManagedBuffer StreamSplitter::getBuffer()
{
    activeChannels++;
    return lastBuffer;
}

void StreamSplitter::periodicCallback() {
    if( this->__cycle++ % 50 != 0 )
        return;

    if( this->id == 64000 ) {
        char const CLEAR_SRC[] = {0x1B, 0x5B, 0x32, 0x4A };
        DMESG( CLEAR_SRC );
    }
    
    DMESG( "%d - Active Channels: %d (active = %d, sampleRate = %d)", this->id, this->activeChannels, this->isActive, (int)this->upstream.getSampleRate() );
    for( int i=0; i<CONFIG_MAX_CHANNELS; i++ ){
        if( this->outputChannels[i] != NULL ) {
            if( this->outputChannels[i]->output != NULL )
                DMESG( "\t- %d [CONN] failed = %d (sampleRate = %d)", i, this->outputChannels[i]->pullAttempts, (int)this->outputChannels[i]->getSampleRate() );
            else
                DMESG( "\t- %d [DISC] failed = %d (sampleRate = %d)", i, this->outputChannels[i]->pullAttempts, (int)this->outputChannels[i]->getSampleRate() );
        } else
            DMESG( "\t- %d [----]", i );
    }
}

/**
 * Callback provided when data is ready.
 */
int StreamSplitter::pullRequest()
{
    if( activeChannels > 0 )
    {
        if( !isActive )
            Event e( id, SPLITTER_ACTIVATE );
        isActive = true;
        lastBuffer = upstream.pull();
    }
    else
    {
        if( isActive )
            Event e( id, SPLITTER_DEACTIVATE );
        isActive = false;
        lastBuffer = ManagedBuffer();
    }
    
    activeChannels = 0;

    // For each downstream channel that exists in array outputChannels - make a pullRequest
    for (int i = 0; i < CONFIG_MAX_CHANNELS; i++)
    {
        if (outputChannels[i] != NULL)
            outputChannels[i]->pullRequest();
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
            break;
        }
    }
    if(placed != -1) {
        channels++;
        Event e( id, SPLITTER_ACTIVATE_CHANNEL );
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
            Event e( id, SPLITTER_DEACTIVATE_CHANNEL );
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
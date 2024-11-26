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

SplitterChannel::SplitterChannel( StreamSplitter * parent, DataSink * output = NULL ) : DataSourceSink(*(new DataSource()))
{
    this->parent = parent;
    this->downStream = output;
}

SplitterChannel::~SplitterChannel()
{
}

ManagedBuffer SplitterChannel::resample( ManagedBuffer _in, uint8_t *buffer, int length ) {
    
    // Fast path. Perform a shallow copy of the input buffer where possible.
    // TODO: verify this is still a safe operation.
    if (this->sampleDropRate == 1)
        return _in;
    
    // Going the long way around - drop any excess samples...
    int inFmt = parent->upstream.getFormat();
    int bytesPerSample = DATASTREAM_FORMAT_BYTES_PER_SAMPLE( inFmt );
    int totalSamples = _in.length() / bytesPerSample;
    int numOutputSamples = (totalSamples / sampleDropRate) + 1;
    uint8_t *outPtr = NULL;

    ManagedBuffer output = ManagedBuffer(numOutputSamples * bytesPerSample);
    outPtr = output.getBytes();

    for (int i = 0; i < totalSamples * bytesPerSample; i++)
    {
        sampleSigma += StreamNormalizer::readSample[inFmt]( &_in[i]);
        sampleDropPosition++;

        if (sampleDropPosition >= sampleDropRate)
        {
            StreamNormalizer::writeSample[inFmt](outPtr, sampleSigma / sampleDropRate);
            outPtr += bytesPerSample;

            sampleDropPosition = 0;
            sampleSigma = 0;
        }
    }

    output.truncate(outPtr - output.getBytes());

    return output;
}

uint8_t * SplitterChannel::pullInto( uint8_t * rawBuffer, int length )
{
    ManagedBuffer inData = parent->getBuffer();
    ManagedBuffer result = this->resample( inData, rawBuffer, length );

    return rawBuffer + result.length();
}

ManagedBuffer SplitterChannel::pull()
{
    ManagedBuffer inData = parent->getBuffer();
    return this->resample( inData ); // Autocreate the output buffer
}

int SplitterChannel::getFormat()
{
    return parent->upstream.getFormat();
}

int SplitterChannel::setFormat(int format)
{
    return parent->upstream.setFormat( format );
}

int SplitterChannel::requestSampleDropRate( int sampleDropRate )
{
    // TODO: Any validaiton to do here? Or do we permit any integer multiple?
    this->sampleDropRate = sampleDropRate;
    this->sampleDropPosition = 0;
    this->sampleSigma = 0;

    return this->sampleDropRate;
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
            }
        }
    }
    
    if( activeChannels == 0 && isActive ) {
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
            if( outputChannels[i]->downStream == output ) {
                return outputChannels[i];
            }
        }
    }

    return NULL;
}

float SplitterChannel::getSampleRate() {
    return parent->upstream.getSampleRate() / sampleDropRate;
}
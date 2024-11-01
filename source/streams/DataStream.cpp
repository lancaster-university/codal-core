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

#include "DataStream.h"
#include "CodalComponent.h"
#include "ManagedBuffer.h"
#include "Event.h"
#include "CodalFiber.h"
#include "ErrorNo.h"

using namespace codal;

/**
* Default implementation of DataSource and DataSink classes.
*/
ManagedBuffer DataSource::pull()
{
	return ManagedBuffer();
}

void DataSource::connect(DataSink& )
{
}

void DataSource::disconnect()
{
}

int DataSource::getFormat()
{
    return DATASTREAM_FORMAT_UNKNOWN;
}

int DataSource::setFormat(int format)
{
    return DEVICE_NOT_SUPPORTED;
}

float DataSource::getSampleRate() {
    return DATASTREAM_SAMPLE_RATE_UNKNOWN;
}

int DataSink::pullRequest()
{
	return DEVICE_NOT_SUPPORTED;
}

DataStream::DataStream(DataSource &upstream)
{
    this->pullRequestEventCode = 0;
    this->isBlocking = true;
    this->hasPending = false;
    this->missedBuffers = CODAL_DATASTREAM_HIGH_WATER_MARK;
    this->downstreamReturn = DEVICE_OK;
    this->flowEventCode = 0;

    this->downStream = NULL;
    this->upStream = &upstream;
}

DataStream::~DataStream()
{
}

uint16_t DataStream::emitFlowEvents( uint16_t id )
{
    if( this->flowEventCode == 0 ) {
        if( id == 0 )
            this->flowEventCode = allocateNotifyEvent();
        else
            this->flowEventCode = id;
    }
    return this->flowEventCode;
}

bool DataStream::isReadOnly()
{
    if( this->hasPending )
        return this->nextBuffer.isReadOnly();
    return true;
}

bool DataStream::isFlowing()
{
    return this->missedBuffers < CODAL_DATASTREAM_HIGH_WATER_MARK;
}

void DataStream::connect(DataSink &sink)
{
	this->downStream = &sink;
    this->upStream->connect(*this);
}

bool DataStream::isConnected()
{
    return this->downStream != NULL;
}

int DataStream::getFormat()
{
    return upStream->getFormat();
}

void DataStream::disconnect()
{
	this->downStream = NULL;
}

void DataStream::setBlocking(bool isBlocking)
{
    this->isBlocking = isBlocking;

    // If this is the first time async mode has been used on this stream, allocate the necessary resources.
    if (!this->isBlocking && this->pullRequestEventCode == 0)
    {
        this->pullRequestEventCode = allocateNotifyEvent();

        if(EventModel::defaultEventBus)
            EventModel::defaultEventBus->listen(DEVICE_ID_NOTIFY, this->pullRequestEventCode, this, &DataStream::onDeferredPullRequest);
    }
}

ManagedBuffer DataStream::pull()
{
    // 1, as we will normally be at '1' waiting buffer here if we're in-sync with the source
    if( this->missedBuffers > 1 )
        Event evt( DEVICE_ID_NOTIFY, this->flowEventCode );
    
    this->missedBuffers = 0;
    // Are we running in sync (blocking) mode?
    if( this->isBlocking )
        return this->upStream->pull();
    
    this->hasPending = false;
    return ManagedBuffer( this->nextBuffer ); // Deep copy!
}

void DataStream::onDeferredPullRequest(Event)
{
    this->downstreamReturn = DEVICE_OK; // The default state

    if (downStream != NULL)
        this->downstreamReturn = downStream->pullRequest();
}

bool DataStream::canPull(int size)
{
    // We only buffer '1' ahead at most, so if we have one already, refuse more
    return this->hasPending;
}

int DataStream::pullRequest()
{
    // _Technically_ not a missed buffer... yet. But we can only check later.
    if( this->missedBuffers < CODAL_DATASTREAM_HIGH_WATER_MARK )
        if( ++this->missedBuffers == CODAL_DATASTREAM_HIGH_WATER_MARK )
            if( this->flowEventCode != 0 )
                Event evt( DEVICE_ID_NOTIFY, this->flowEventCode );

    // Are we running in async (non-blocking) mode?
    if( !this->isBlocking ) {
        if( this->hasPending && this->downstreamReturn != DEVICE_OK ) {
            Event evt( DEVICE_ID_NOTIFY, this->pullRequestEventCode );
            return this->downstreamReturn;
        }

        this->nextBuffer = this->upStream->pull();
        this->hasPending = true;

        Event evt( DEVICE_ID_NOTIFY, this->pullRequestEventCode );
        return this->downstreamReturn;
    }

    if( this->downStream != NULL )
        return this->downStream->pullRequest();

    return DEVICE_BUSY;
}

float DataStream::getSampleRate() {
    if( this->upStream != NULL )
        return this->upStream->getSampleRate();
    return DATASTREAM_SAMPLE_RATE_UNKNOWN;
}

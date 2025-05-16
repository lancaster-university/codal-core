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
#include "CodalDmesg.h"

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

void DataSource::dataWanted(int wanted)
{
    dataIsWanted = wanted;
}

int DataSource::isWanted()
{
    return dataIsWanted;
}

//DataSink methods.
int DataSink::pullRequest()
{
	return DEVICE_NOT_SUPPORTED;
}

// DataSourceSink methods.
DataSourceSink::DataSourceSink(DataSource &source) : upStream( source )
{
    downStream = NULL;
    source.connect( *this );
    dataWanted(DATASTREAM_DONT_CARE);
}

DataSourceSink::~DataSourceSink()
{
}

void DataSourceSink::connect(DataSink &sink)
{
    downStream = &sink;
}

bool DataSourceSink::isConnected()
{ 
    return downStream != NULL;
}

void DataSourceSink::disconnect()
{
    downStream = NULL;
}

int DataSourceSink::getFormat()
{
    return upStream.getFormat();
}

int DataSourceSink::setFormat(int format)
{
    return upStream.setFormat( format );
}

float DataSourceSink::getSampleRate()
{
    return upStream.getSampleRate();
}

void DataSourceSink::dataWanted(int wanted)
{
    DataSource::dataWanted(wanted);
    upStream.dataWanted(wanted);
}

int DataSourceSink::pullRequest()
{
    if( this->downStream != NULL )
        return this->downStream->pullRequest();
    return DEVICE_BUSY;
}

/**
 * Definition for a DataStream class. This doesn't *really* belong in here, as its key role is to
 * decouple a pipeline the straddles an interrupt context boundary...
 */
DataStream::DataStream(DataSource &upstream) : DataSourceSink(upstream)
{
    this->pullRequestEventCode = 0;
    this->isBlocking = true;
    this->hasPending = false;
}

DataStream::~DataStream()
{
}

bool DataStream::isReadOnly()
{
    if( this->hasPending )
        return this->nextBuffer.isReadOnly();
    return true;
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
    // Are we running in sync (blocking) mode?
    if( this->isBlocking )
        return this->upStream.pull();
    
    this->hasPending = false;
    return ManagedBuffer( this->nextBuffer ); // Deep copy!
}

void DataStream::onDeferredPullRequest(Event)
{
    if (downStream != NULL)
        downStream->pullRequest();
}

bool DataStream::canPull(int size)
{
    // We only buffer '1' ahead at most, so if we have one already, refuse more
    return this->hasPending;
}

int DataStream::pullRequest()
{
    // Are we running in async (non-blocking) mode?
    if( !this->isBlocking ) {

        this->nextBuffer = this->upStream.pull();
        this->hasPending = true;

        Event evt( DEVICE_ID_NOTIFY, this->pullRequestEventCode );
        return DEVICE_OK;
    }

    if( this->downStream != NULL )
        return this->downStream->pullRequest();

    return DEVICE_BUSY;
}

void DataStream::connect(DataSink &sink)
{
    DMESG("CONNECT REQUEST: this: %p, sink: %p", this, &sink);
    this->downStream = &sink;
}
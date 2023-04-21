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

float DataSource::requestSampleRate(float sampleRate) {
    // Just consume this by default, we don't _have_ to honour requests for specific rates.
    return DATASTREAM_SAMPLE_RATE_UNKNOWN;
}

int DataSink::pullRequest()
{
	return DEVICE_NOT_SUPPORTED;
}


/**
  * Class definition for DataStream.
  * A Datastream holds a number of ManagedBuffer references, provides basic flow control through a push/pull mechanism
  * and byte level access to the datastream, even if it spans different buffers.
  */
DataStream::DataStream(DataSource &upstream)
{
    //this->bufferCount = 0;
    //this->bufferLength = 0;
    //this->preferredBufferSize = 0;
    //this->pullRequestEventCode = 0;
    //this->spaceAvailableEventCode = allocateNotifyEvent();
    this->nextBuffer = NULL;
    this->isBlocking = true;
    //this->writers = 0;

    this->downStream = NULL;
    this->upStream = &upstream;
}

/**
 * Destructor.
 * Removes all resources held by the instance.
 */
DataStream::~DataStream()
{
}

/**
 * Determines the value of the given byte in the buffer.
 *
 * @param position The index of the byte to read.
 * @return The value of the byte at the given position, or DEVICE_INVALID_PARAMETER.
 */
/*int DataStream::get(int position)
{
	for (int i = 0; i < bufferCount; i++)
	{
		if (position < stream[i].length())
			return stream[i].getByte(position);

		position = position - stream[i].length();
	}

	return DEVICE_INVALID_PARAMETER;
}*/

/**
 * Sets the byte at the given index to value provided.
 * @param position The index of the byte to change.
 * @param value The new value of the byte (0-255).
 * @return DEVICE_OK, or DEVICE_INVALID_PARAMETER.
 *
 */
/*int DataStream::set(int position, uint8_t value)
{
	for (int i = 0; i < bufferCount; i++)
	{
		if (position < stream[i].length())
		{
			stream[i].setByte(position, value);
			return DEVICE_OK;
		}

		position = position - stream[i].length();
	}

	return DEVICE_INVALID_PARAMETER;
}*/

/**
 * Gets number of bytes that are ready to be consumed in this data stream.
 * @return The size in bytes.
 */
/*int DataStream::length()
{
	return this->bufferLength;
}*/

/**
 * Determines if any of the data currently flowing through this stream is held in non-volatile (FLASH) memory.
 * @return true if one or more of the ManagedBuffers in this stream reside in FLASH memory, false otherwise.
 */
bool DataStream::isReadOnly()
{
    /*bool r = true;

    for (int i=0; i<bufferCount;i++)
        if (stream[i].isReadOnly() == false)
            r = false;

    return r;*/

    if( this->nextBuffer != NULL )
        return nextBuffer->isReadOnly();
    return true;
}

/**
 * Define a downstream component for data stream.
 *
 * @sink The component that data will be delivered to, when it is available
 */
void DataStream::connect(DataSink &sink)
{
	this->downStream = &sink;
    this->upStream->connect(*this);
}

/**
 *  Determine the data format of the buffers streamed out of this component.
 */
int DataStream::getFormat()
{
    return upStream->getFormat();
}

/**
 * Define a downstream component for data stream.
 *
 * @sink The component that data will be delivered to, when it is available
 */
void DataStream::disconnect()
{
	this->downStream = NULL;
}

/**
 * Determine the number of bytes that are currnetly buffered before blocking subsequent push() operations.
 * @return the current preferred buffer size for this DataStream
 */
/*int DataStream::getPreferredBufferSize()
{
	return preferredBufferSize;
}*/

/**
 * Define the number of bytes that should be buffered before blocking subsequent push() operations.
 * @param size The number of bytes to buffer.
 */
/*void DataStream::setPreferredBufferSize(int size)
{
	this->preferredBufferSize = size;
}*/

/**
 * Determines if this stream acts in a synchronous, blocking mode or asynchronous mode. In blocking mode, writes to a full buffer
 * will result in the calling fiber being blocked until space is available. Downstream DataSinks will also attempt to process data
 * immediately as it becomes available. In non-blocking asynchronous mode, writes to a full buffer are dropped and processing of
 * downstream Datasinks will be deferred.
 */
void DataStream::setBlocking(bool isBlocking)
{
    this->isBlocking = isBlocking;

    // If this is the first time async mode has been used on this stream, allocate the necessary resources.
    /*if (!isBlocking && this->pullRequestEventCode == 0)
    {
        this->pullRequestEventCode = allocateNotifyEvent();

        if(EventModel::defaultEventBus)
            EventModel::defaultEventBus->listen(DEVICE_ID_NOTIFY, pullRequestEventCode, this, &DataStream::onDeferredPullRequest);
    }*/
}

/**
 * Provide the next available ManagedBuffer to our downstream caller, if available.
 */
ManagedBuffer DataStream::pull()
{
	/*ManagedBuffer out = stream[0];

	//
	// A simplistic FIFO for now. Copy cost is actually pretty low because ManagedBuffer is a managed type,
	// so we're just moving a few references here.
	//
	if (bufferCount > 0)
	{
		for (int i = 0; i < bufferCount-1; i++)
			stream[i] = stream[i + 1];

        stream[bufferCount-1] = ManagedBuffer();

		bufferCount--;
		bufferLength = bufferLength - out.length();
	}

    Event(DEVICE_ID_NOTIFY_ONE, spaceAvailableEventCode);

	return out;*/

    /*if( this->nextBuffer != NULL )
        return *this->nextBuffer;

    return ManagedBuffer();*/

    return this->upStream->pull();
}

/**
 * Issue a pull request to our downstream component, if one has been registered.
 */
/*void DataStream::onDeferredPullRequest(Event)
{
    if (downStream != NULL)
        downStream->pullRequest();
}*/

/**
 * Determines if a buffer of the given size can be added to the buffer.
 *
 * @param size The number of bytes to add to the buffer.
 * @return true if there is space for "size" bytes in the buffer. false otherwise.
 */
bool DataStream::canPull(int size)
{
    DMESG( "DataStream::canPull()" );
    /*if(bufferCount + writers >= DATASTREAM_MAXIMUM_BUFFERS)
        return false;

    if(preferredBufferSize > 0 && (bufferLength + size > preferredBufferSize))
        return false;

    return true;*/

    return this->nextBuffer != NULL;
}

/**
 * Determines if the DataStream can accept any more data.
 *
 * @return true if there if the buffer is ful, and can accept no more data at this time. False otherwise.
 */
/*bool DataStream::full()
{
    return !canPull();
}*/

/**
 * Store the given buffer in our stream, possibly also causing a push operation on our downstream component.
 */
int DataStream::pullRequest()
{
    /*
    // If we're defined as non-blocking and no space is available, then there's nothing we can do.
    if (full() && this->isBlocking == false)
        return DEVICE_NO_RESOURCES;

    // As there is either space available in the buffer or we want to block, pull the upstream buffer to release resources there.
    ManagedBuffer buffer = upStream->pull();

    // If pull is called multiple times in a row (yielding nothing after the first time)
    // several streams might be woken up, despite the fact that there is no space for them.
    do {
        // If the buffer is full or we're behind another fiber, then wait for space to become available.
        if (full() || writers)
            fiber_wake_on_event(DEVICE_ID_NOTIFY, spaceAvailableEventCode);

        if (full() || writers)
        {
            writers++;
            schedule();
            writers--;
        }
    } while (bufferCount >= DATASTREAM_MAXIMUM_BUFFERS);

	stream[bufferCount] = buffer;
	bufferLength = bufferLength + buffer.length();
	bufferCount++;

	if (downStream != NULL)
    {
        DMESG( "DS(blocking = %d) -> PR?", this->isBlocking );
        if (this->isBlocking) {
            DMESG( "DS -> Direct PR" );
            downStream->pullRequest();
        }
        else {
            DMESG( "DS -> Defer PR" );
            Event(DEVICE_ID_NOTIFY, pullRequestEventCode);
        }
        
    }

	return DEVICE_OK;*/

    if( this->downStream != NULL ) {
        this->downStream->pullRequest();
    }

    return DEVICE_OK;
}

/**
 * Gets the sample rate for the stream that this component is part of.
 * 
 * This will query 'up stream' towards the stream source to determine the current sample rate.
 * 
 * @return float The sample rate, in samples-per-second, or DATASTREAM_SAMPLE_RATE_UNKNOWN if not known or unconfigured
 */
float DataStream::getSampleRate() {
    if( this->upStream != NULL )
        return this->upStream->getSampleRate();
    return DATASTREAM_SAMPLE_RATE_UNKNOWN;
}

/**
 * Sents a request 'up stream' towards the stream source for a higher sample rate, if possible.
 * 
 * @note Components are not required to respect this request, and this mechanism is best-effort, so if the caller needs a specific rate, it should check via getSampleRate after making this request.
 * 
 * @param sampleRate The sample rate requestd, in samples-per-second
 */
float DataStream::requestSampleRate(float sampleRate) {
    if( this->upStream != NULL )
        return this->upStream->requestSampleRate( sampleRate );
    return DATASTREAM_SAMPLE_RATE_UNKNOWN;
}
#include "StreamRecording.h"
#include "ErrorNo.h"
#include "DataStream.h"
#include "ManagedBuffer.h"
#include "MessageBus.h"

using namespace codal;

StreamRecording::StreamRecording( DataSource &source, uint32_t maxLength ) : upStream( source )
{   
    this->state = REC_STATE_STOPPED;
    this->bufferChain = NULL;
    this->lastBuffer = NULL;
    this->readHead = NULL;
    this->maxBufferLenth = maxLength;
    this->totalBufferLength = 0;

    this->downStream = NULL;
    upStream.connect( *this );
}

StreamRecording::~StreamRecording()
{
    //
}

bool StreamRecording::canPull()
{
    return this->totalBufferLength < this->maxBufferLenth;
}

ManagedBuffer StreamRecording::pull()
{
    // Are we playing back?
    if( this->state != REC_STATE_PLAYING )
        return ManagedBuffer();
    
    // Do we have data to send?
    if( this->readHead == NULL ) {
        stop();
        return ManagedBuffer();
    }
    
    // Grab the next block and move the r/w head
    ManagedBuffer out = this->readHead->buffer;
    this->readHead = this->readHead->next;

    // Prod the downstream that we're good to go
    if( downStream != NULL )
        downStream->pullRequest();

    // Return the block
    return out;
}

int StreamRecording::length()
{
    return this->totalBufferLength;
}

float StreamRecording::duration( unsigned int sampleRate )
{
    return ((float)this->length() / DATASTREAM_FORMAT_BYTES_PER_SAMPLE((float)this->getFormat()) ) / (float)sampleRate;
}

bool StreamRecording::isFull() {
    return this->totalBufferLength >= this->maxBufferLenth;
}

int StreamRecording::pullRequest()
{
    // Are we recording?
    if( this->state != REC_STATE_RECORDING )
        return DEVICE_BUSY;

    ManagedBuffer data = this->upStream.pull();

    // Are we getting empty buffers (probably because we're out of RAM!)
    if( data == ManagedBuffer() || data.length() <= 1 ) {
        return DEVICE_OK;
    }

    // Can we record any more?
    if( !isFull() )
    {
        StreamRecording_Buffer * block = new StreamRecording_Buffer();
        if( block == NULL )
            return DEVICE_NO_RESOURCES;
        block->buffer = data;
        block->next = NULL;

        // Are we initialising stuff? If so, hook the front of the chain up too...
        if( this->lastBuffer == NULL ) {
            this->bufferChain = block;
        } else
            this->lastBuffer->next = block;
        
        this->lastBuffer = block;
        
        this->totalBufferLength += this->lastBuffer->buffer.length();
        
        return DEVICE_OK;
    }
    
    this->stop();
    return DEVICE_NO_RESOURCES;
}

void StreamRecording::connect( DataSink &sink )
{
    this->downStream = &sink;
}

bool StreamRecording::isConnected()
{
    return this->downStream != NULL;
}

void StreamRecording::disconnect()
{
    this->downStream = NULL;
}

int StreamRecording::getFormat()
{
    return this->upStream.getFormat();
}

int StreamRecording::setFormat( int format )
{
    return this->upStream.setFormat( format );
}

bool StreamRecording::recordAsync()
{
    // Duplicate check from within erase(), but here for safety in case of later code edits...
    if( this->state != REC_STATE_STOPPED )
        this->stop();
    
    erase();

    bool changed = this->state != REC_STATE_RECORDING;

    this->state = REC_STATE_RECORDING;

    return changed;
}

void StreamRecording::record()
{
    recordAsync();
    while( isRecording() )
        fiber_sleep(5);
}

void StreamRecording::erase()
{
    if( this->state != REC_STATE_STOPPED )
        this->stop();
    
    // Run down the chain, freeing as we go
    StreamRecording_Buffer * node = this->bufferChain;
    while( node != NULL ) {
        StreamRecording_Buffer * next = node->next;
        delete node;
        node = next;
    }
    this->totalBufferLength = 0;
    this->lastBuffer = NULL;
    this->readHead = NULL;
    this->bufferChain = NULL;
}

bool StreamRecording::playAsync()
{
    if( this->state != REC_STATE_STOPPED )
        this->stop();
    bool changed = this->state != REC_STATE_PLAYING;
    
    this->state = REC_STATE_PLAYING;
    if( this->downStream != NULL )
        this->downStream->pullRequest();

    return changed;
}

void StreamRecording::play()
{
    playAsync();
    while( isPlaying() )
        fiber_sleep(5);
}

bool StreamRecording::stop()
{
    bool changed = this->state != REC_STATE_STOPPED;

    this->state = REC_STATE_STOPPED;
    this->readHead = this->bufferChain; // Snap to the start

    return changed;
}

bool StreamRecording::isPlaying()
{
    fiber_sleep(0);
    return this->state == REC_STATE_PLAYING;
}

bool StreamRecording::isRecording()
{
    fiber_sleep(0);
    return this->state == REC_STATE_RECORDING;
}

bool StreamRecording::isStopped()
{
    fiber_sleep(0);
    return this->state == REC_STATE_STOPPED;
}

float StreamRecording::getSampleRate()
{
    return this->upStream.getSampleRate();
}
#include "StreamRecording.h"
#include "ErrorNo.h"
#include "DataStream.h"
#include "ManagedBuffer.h"
#include "MessageBus.h"
#include "CodalDmesg.h"

using namespace codal;

// Minimum memory overhead of adding a buffer to the chain
// StreamRecording_Buffer contains a ManagedBuffer which points to a BufferData
// StreamRecording_Buffer and BufferData are heap blocks with a PROCESSOR_WORD_TYPE overhead
#define CODAL_STREAM_RECORDING_BUFFER_OVERHEAD \
  ( sizeof(StreamRecording_Buffer) + sizeof(BufferData) + 2 * sizeof(PROCESSOR_WORD_TYPE))


StreamRecording::StreamRecording( DataSource &source, uint32_t maxLength ) : DataSourceSink( source ) 
{   
    this->state = REC_STATE_STOPPED;

    // The test for "full" was totalBufferLength >= maxBufferLenth
    // Adjust this number by the memory overhead
    // of the old default case with buffers of 256 bytes.
    this->maxBufferLenth = maxLength + ( maxLength / 256 + 1) * CODAL_STREAM_RECORDING_BUFFER_OVERHEAD;

    initialise();
}

StreamRecording::~StreamRecording()
{
}

void StreamRecording::initialise()
{
    this->totalBufferLength = 0;
    this->totalMemoryUsage = 0;
    this->lastBuffer = NULL;
    this->readHead = NULL;
    this->bufferChain = NULL;
    this->lastUpstreamRate = DATASTREAM_SAMPLE_RATE_UNKNOWN;
}

bool StreamRecording::canPull()
{
    return this->totalMemoryUsage < this->maxBufferLenth;
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
    return this->totalMemoryUsage >= this->maxBufferLenth;
}

void StreamRecording::printChain()
{
    #if CONFIG_ENABLED(DMESG_SERIAL_DEBUG) && CONFIG_ENABLED(DMESG_AUDIO_DEBUG)
        DMESGN( "START -> " );
        StreamRecording_Buffer * node = this->bufferChain;
        while( node != NULL ) {
            DMESGN( "%x -> ", (int)(node->buffer.getBytes()) );
            codal_dmesg_flush();
            node = node->next;
        }
        DMESG( "END (%d hz)", (int)this->lastUpstreamRate );
    #endif
}

int StreamRecording::pullRequest()
{
    // Are we recording?
    if( this->state != REC_STATE_RECORDING )
        return DEVICE_BUSY;

    ManagedBuffer data = this->upStream.pull();
    this->lastUpstreamRate = this->upStream.getSampleRate();

    // Are we getting empty buffers (probably because we're out of RAM!)
    if( data == ManagedBuffer() || data.length() <= 1 ) {
        return DEVICE_OK;
    }

    // Can we record any more?
    if( !isFull() )
    {
        StreamRecording_Buffer * block = new StreamRecording_Buffer( data );
        if( block == NULL )
            return DEVICE_NO_RESOURCES;
        block->next = NULL;

        // Are we initialising stuff? If so, hook the front of the chain up too...
        if( this->lastBuffer == NULL ) {
            this->bufferChain = block;
        } else
            this->lastBuffer->next = block;
        
        this->lastBuffer = block;
        
        uint32_t length = this->lastBuffer->buffer.length();
        this->totalBufferLength += length;
        this->totalMemoryUsage  += length + CODAL_STREAM_RECORDING_BUFFER_OVERHEAD;
        return DEVICE_OK;
    }
    
    this->stop();
    return DEVICE_NO_RESOURCES;
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
    printChain();
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
    initialise();
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

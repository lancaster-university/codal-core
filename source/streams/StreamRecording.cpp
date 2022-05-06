#include "StreamRecording.h"
#include "ErrorNo.h"
#include "DataStream.h"
#include "ManagedBuffer.h"
#include "CodalDmesg.h"
#include "MessageBus.h"

using namespace codal;

StreamRecording::StreamRecording( DataSource &source ) : upStream( source )
{   
    this->bufferLength = 0;
    this->lastBuffer = 0;
    this->readWriteHead = 0;

    this->downStream = NULL;
    source.connect( *this );
}

StreamRecording::~StreamRecording()
{
    //
}

bool StreamRecording::canPull()
{
    return this->lastBuffer > this->readWriteHead;
}

ManagedBuffer StreamRecording::pull()
{
    // Are we playing back?
    if( this->state != REC_STATE_PLAYING )
        return ManagedBuffer();
    
    // Do we have data to send?
    if( this->readWriteHead >= this->lastBuffer ) {
        stop();
        return ManagedBuffer();
    }
    
    // Grab the next block
    ManagedBuffer out = this->buffer[this->readWriteHead++];
    this->bufferLength -= out.length();

    // Ping the downstream that we're good to go
    if( downStream != NULL )
        downStream->pullRequest();

    // Return the block
    return out;
}

int StreamRecording::length()
{
    return this->bufferLength;
}

float StreamRecording::duration( unsigned int sampleRate )
{
    return ((float)this->length() / DATASTREAM_FORMAT_BYTES_PER_SAMPLE((float)this->getFormat()) ) / (float)sampleRate;
}

bool StreamRecording::isFull() {
    return this->lastBuffer < REC_MAX_BUFFERS;
}

void StreamRecording::dumpState()
{
    DMESG(
        "TapeDeck { bufferCount = %d/%d, RWHead = %d }",
        this->lastBuffer,
        REC_MAX_BUFFERS,
        this->readWriteHead
    );
}

int StreamRecording::pullRequest()
{
    ManagedBuffer data = this->upStream.pull();

    // Are we recording?
    if( this->state != REC_STATE_RECORDING )
        return DEVICE_OK;
    
    // Are we getting empty buffers (probably because we're out of RAM!)
    if( data.length() == 0 )
        return DEVICE_NO_RESOURCES;

    // Can we record any more?
    if( this->readWriteHead < REC_MAX_BUFFERS )
    {
        // Ok, so pull and retain, updating counts
        this->buffer[this->readWriteHead++] = data;
        this->lastBuffer = this->readWriteHead - 1;
        this->bufferLength += data.length();
        return DEVICE_OK;
    }
    
    this->stop();
    return DEVICE_NO_RESOURCES;
}

void StreamRecording::connect( DataSink &sink )
{
    this->downStream = &sink;
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

bool StreamRecording::record()
{
    // Duplicate check from within erase(), but here for safety in case of later code edits...
    if( this->state != REC_STATE_STOPPED )
        this->stop();
    
    erase();

    bool changed = this->state != REC_STATE_RECORDING;

    this->state = REC_STATE_RECORDING;
    this->upStream.pull(); // Kickstart the upstream??

    return changed;
}

void StreamRecording::erase()
{
    if( this->state != REC_STATE_STOPPED )
        this->stop();
    
    for( int i=0; i<REC_MAX_BUFFERS; i++ )
        this->buffer[i] = ManagedBuffer();
    this->lastBuffer = 0;
    this->readWriteHead = 0;
}

bool StreamRecording::play()
{
    if( this->state != REC_STATE_STOPPED )
        this->stop();
    bool changed = this->state != REC_STATE_PLAYING;
    
    this->state = REC_STATE_PLAYING;
    this->downStream->pullRequest();

    return changed;
}

void StreamRecording::stop()
{
    this->state = REC_STATE_STOPPED;
    this->readWriteHead = 0; // Snap to the start
}

bool StreamRecording::isPlaying()
{
    return this->state == REC_STATE_PLAYING;
}

bool StreamRecording::isRecording()
{
    return this->state == REC_STATE_RECORDING;
}

bool StreamRecording::isStopped()
{
    return this->state == REC_STATE_STOPPED;
}
#include "FIFOStream.h"
#include "ErrorNo.h"
#include "DataStream.h"
#include "ManagedBuffer.h"
#include "CodalDmesg.h"
#include "MessageBus.h"

using namespace codal;

FIFOStream::FIFOStream( DataSource &source ) : DataSourceSink( source )
{
    this->bufferCount = 0;
    this->bufferLength = 0;
    
    this->downStream = NULL;
    source.connect( *this );

    this->allowInput = false;
    this->allowOutput = false;

}

FIFOStream::~FIFOStream()
{
}

bool FIFOStream::canPull()
{
    return (this->bufferLength > 0) && this->allowOutput;
}

ManagedBuffer FIFOStream::pull()
{
    if( (this->bufferLength > 0) && this->allowOutput )
    {
        ManagedBuffer out = buffer[0];

        for (int i = 0; i < FIFO_MAXIMUM_BUFFERS-1; i++)
            buffer[i] = buffer[i + 1];

        buffer[FIFO_MAXIMUM_BUFFERS-1] = ManagedBuffer();

        this->bufferLength -= out.length();
        this->bufferCount--;

        if (this->bufferCount > 0 && downStream != NULL)
            downStream->pullRequest();

        return out;
    }

    return ManagedBuffer();
}

int FIFOStream::length()
{
    return this->bufferLength;
}

bool FIFOStream::isFull() {
    return this->bufferCount < FIFO_MAXIMUM_BUFFERS;
}

void FIFOStream::dumpState()
{
    DMESG(
        "TapeDeck { bufferCount = %d/%d, bufferLength = %dB }",
        this->bufferCount,
        FIFO_MAXIMUM_BUFFERS,
        this->bufferLength
    );
}

int FIFOStream::pullRequest()
{
    if( this->bufferCount >= FIFO_MAXIMUM_BUFFERS )
        return DEVICE_NO_RESOURCES;

    ManagedBuffer inBuffer = this->upStream.pull();
    if( this->allowInput && inBuffer.length() > 0 )
    {
        this->buffer[ this->bufferCount++ ] = inBuffer;
        this->bufferLength += inBuffer.length();
    }

    if (bufferCount > 0 && this->allowOutput && downStream != NULL)
        return downStream->pullRequest();

    if( this->bufferCount >= FIFO_MAXIMUM_BUFFERS )
        return DEVICE_BUSY;
    return DEVICE_OK;
}

void FIFOStream::setInputEnable( bool state )
{
    this->allowInput = state;
}
void FIFOStream::setOutputEnable( bool state )
{
    bool enabling = false;
    DMESG("FIFO:setOutputEnable %d", state );

    if (this->allowOutput == false && state)
        enabling = true;

    this->allowOutput = state;
    
    // If we've just been enabled and have data to send, issue a pullrequest to ensure our downstream is aware of this
    if (enabling && bufferCount > 0 && downStream != NULL)
        downStream->pullRequest();
}
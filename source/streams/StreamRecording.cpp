#include "StreamRecording.h"
#include "ErrorNo.h"
#include "DataStream.h"
#include "ManagedBuffer.h"
#include "MessageBus.h"
#include "CodalDmesg.h"

using namespace codal;

StreamRecording::StreamRecording(DataSource &source) : DataSourceSink( source ), recordLock(0, FiberLockMode::MUTEX), playLock(0, FiberLockMode::MUTEX)
{   
    this->state = REC_STATE_STOPPED;
    this->totalBufferLength = 0;
    this->readOffset = 0;
    this->writeOffset = 0;
}

ManagedBuffer StreamRecording::pull()
{
    ManagedBuffer out;

    DMESG("PULL");

    if( state == REC_STATE_PLAYING && readOffset < CODAL_STREAM_RECORDING_SIZE)
        out = data[readOffset++];

    // Wake any blocked threads once we reach the end of the playback
    if (out.length() == 0)
    {
        state = REC_STATE_STOPPED;
        playLock.notifyAll();
    }
    else
        // Indicate to the downstream that another buffer is available.
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
    return ((float)this->length() / (float)DATASTREAM_FORMAT_BYTES_PER_SAMPLE(this->getFormat()) ) / (float)sampleRate;
}

int StreamRecording::pullRequest()
{
    //DMESG("PR... [FORMAT: %d] [BITRATE: %d]", upStream.getFormat(), upStream.getSampleRate());

    // Ignore incoming buffers if we aren't actively recording
    if( this->state != REC_STATE_RECORDING )
        return DEVICE_OK;

    ManagedBuffer buffer = upStream.pull();

    // Ignore any empty buffers (possibly because we're out of RAM!)
    if(buffer.length() == 0)
        return DEVICE_OK;

    // Store the data in our buffer, if we have space
    if (writeOffset < CODAL_DEFAULT_STREAM_RECORDING_MAX_LENGTH)
    {
        // There is space. Determine if we want to store the buffer or copy it.
        if (buffer.length() < CODAL_STREAM_RECORDING_BUFFER_SIZE)
        {
            // Buffer is below our threshold. Copy the data.
            int length = buffer.length();
            int input_offset = 0;
            while (length > 0)
            {
                int b = writeOffset / CODAL_STREAM_RECORDING_BUFFER_SIZE;
                int o = writeOffset % CODAL_STREAM_RECORDING_BUFFER_SIZE;
                int l = CODAL_STREAM_RECORDING_BUFFER_SIZE - o;

                DMESG("Buffer length is under threshold: [buffer.length(): %d]", buffer.length());
                DMESG("[writeOffset: %d][b: %d] [o:%d] [l:%d]", writeOffset, b, o, l);

                if (b < CODAL_STREAM_RECORDING_SIZE)
                {
                    // Allocate memory for the buffer if needed.
                    if (data[b].length() != CODAL_STREAM_RECORDING_BUFFER_SIZE)
                        data[b] = ManagedBuffer(CODAL_STREAM_RECORDING_BUFFER_SIZE);

                    // Copy in the data from the input buffer.
                    if (data[b].length() == CODAL_STREAM_RECORDING_BUFFER_SIZE)
                    {
                        memcpy(data[b].getBytes() + o, buffer.getBytes()+input_offset, l);
                        length -= l;
                        input_offset += l;
                        writeOffset += l;
                        totalBufferLength += l;
                    }else{
                        // We couldn't allocate the necessary buffer resources. Terminate early.
                        length = 0;
                        stop();
                    }
                }
            }
        }else{
            // The incoming buffer is sufficiently large. Just store it.
            int b = writeOffset / CODAL_STREAM_RECORDING_BUFFER_SIZE;
            DMESG("Buffer length is over threshold: [buffer.length(): %d] [b:%d]", buffer.length(), b);
            if (b < CODAL_STREAM_RECORDING_SIZE)
            {
                data[b] = buffer;
                writeOffset += buffer.length();
                totalBufferLength += buffer.length();
            }
        }
    }
    else
    {
        stop();
        DMESG("STOPPING....");
    }

    return DEVICE_OK;
}

int StreamRecording::recordAsync()
{
    DMESG("RECORDING...");
    // If we're already recording, then treat as a NOP.
    if(state != REC_STATE_RECORDING)
    {
        // We could be playing back. If so, stop first and erase our buffer.
        DMESG("STOPPING...");
        stop();
        DMESG("ERASING...");
        erase();

        state = REC_STATE_RECORDING;
        dataWanted(DATASTREAM_WANTED);

        DMESG("DONE...");
    }

    return DEVICE_OK;
}

void StreamRecording::record()
{
    recordAsync();
    recordLock.wait();
}

void StreamRecording::erase()
{
    if( this->state != REC_STATE_STOPPED )
        this->stop();
    
    // Erase current buffer
    for (int i = 0; i < CODAL_STREAM_RECORDING_SIZE; i++)
        data[i] = ManagedBuffer();

    // Set length
    totalBufferLength = 0;
    readOffset = 0;
    writeOffset = 0;
}

int StreamRecording::playAsync()
{
    DMESG("PLAY_ASYNC");
    if( this->state != REC_STATE_PLAYING )
    {
        this->state = REC_STATE_PLAYING;
        readOffset = 0;

        if( this->downStream != NULL )
            this->downStream->pullRequest();
    }

    return DEVICE_OK;
}

void StreamRecording::play()
{
    playAsync();
    playLock.wait();
}

int StreamRecording::stop()
{
    if (this->state != REC_STATE_STOPPED)
    {
        DMESG("STOPPING RECORDING");

        this->state = REC_STATE_STOPPED;
        dataWanted(DATASTREAM_DONT_CARE);
        recordLock.notifyAll();
    }

    this->readOffset = 0;

    return DEVICE_OK;
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

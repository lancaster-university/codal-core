#ifndef RECORDING_STREAM_H
#define RECORDING_STREAM_H

#include "ManagedBuffer.h"
#include "DataStream.h"

// Pretty much the largest sensible number we can have on a Micro:bit v2
#ifndef CODAL_DEFAULT_STREAM_RECORDING_MAX_LENGTH
    #define CODAL_DEFAULT_STREAM_RECORDING_MAX_LENGTH      50000 // 50k, in bytes
#endif

#define REC_STATE_STOPPED   0
#define REC_STATE_PLAYING   1
#define REC_STATE_RECORDING 2

namespace codal
{

    class StreamRecording_Buffer {
        public:
        ManagedBuffer buffer;
        StreamRecording_Buffer * next;

        StreamRecording_Buffer( ManagedBuffer data ) {
            this->buffer = data;
            this->next = NULL;
        }
    };

    class StreamRecording : public DataSourceSink
    {
        private:

        StreamRecording_Buffer * lastBuffer;
        StreamRecording_Buffer * readHead;
        uint32_t maxBufferLenth;
        uint32_t totalBufferLength;
        uint32_t totalMemoryUsage;
        int state;
        float lastUpstreamRate;

        void initialise();

        public:

        StreamRecording_Buffer * bufferChain;

        /**
         * @brief Construct a new Stream Recording object
         * 
         * @param source An upstream DataSource to connect to
         * @param length The maximum amount of memory (RAM) in bytes to allow this recording object to use. Defaults to CODAL_DEFAULT_STREAM_RECORDING_MAX_LENGTH.
         */
        StreamRecording( DataSource &source, uint32_t length = CODAL_DEFAULT_STREAM_RECORDING_MAX_LENGTH );

        /**
         * @brief Destroy the Stream Recording object
         */
        ~StreamRecording();

        virtual ManagedBuffer pull();
        virtual int pullRequest();

        void printChain();

        /**
         * @brief Calculate and return the length <b>in bytes</b> that this StreamRecording represents
         * 
         * @return int The length, in bytes.
         */
        int length();

        /**
         * @brief Calculate the recorded duration for this StreamRecording.
         * 
         * As this cannot be known by this class (as the sample rate may change during playback <i>or</i> recording) the expected rate must be supplied.
         * 
         * @param sampleRate The sample rate to calculate the duration for, in samples per second.
         * @return long The total duration of this StreamRecording, based on the supplied sample rate, in seconds.
         */
        float duration( unsigned int sampleRate );

        /**
         * @brief Downstream classes should use this to determing if there is data to pull from this StreamRecording object.
         * 
         * @return true If data is available
         * @return false If the object is completely empty
         */
        bool canPull();

        /**
         * @brief Checks if this object can store any further ManagedBuffers from the upstream components.
         * 
         * @note This does <b>not</b> mean that RAM is completely full, but simply that there is now more internal storage for ManagedBuffer references.
         * 
         * @return true If there are no more slots available to track more ManagedBuffers.
         * @return false If there is remaining internal storage capacity for more data
         */
        bool isFull();

        /**
         * @brief Begin recording data from the connected upstream
         * 
         * The StreamRecording object will, if already playing; stop playback, erase its buffer, and start recording.
         * 
         * Non-blocking, will return immediately.
         * 
         * @return Returns true if the object state actually changed (ie. we weren't already recording)
         */
        bool recordAsync();

        /**
         * @brief Begin recording data from the connected upstream
         * 
         * The StreamRecording object will, if already playing; stop playback, erase its buffer, and start recording.
         * 
         * Blocking call, will repeatedly deschedule the current fiber until the recording completes.
         */
        void record();

        /**
         * @brief Begin playing data from the connected upstream
         * 
         * The StreamRecording object will, if already recording; stop recording, rewind to the start of its buffer, and start playing.
         * 
         * Non-blocking, will return immediately.
         * 
         * @return Returns true if the object state actually changed (ie. we weren't already recording)
         */
        bool playAsync();

        /**
         * @brief Begin playing data from the connected upstream
         * 
         * The StreamRecording object will, if already recording; stop recording, rewind to the start of its buffer, and start playing.
         * 
         * Blocking call, will repeatedly deschedule the current fiber until the playback completes.
         */
        void play();

        /**
         * @brief Stop recording or playing the data stored in this StreamRecording object.
         * 
         * Repeated calls to this will do nothing if the object is not in a recording or playback state.
         * 
         * @return Do not use this value, return semantics are changing.
         */
        bool stop();

        /**
         * @brief Erase the internal buffer.
         * 
         * Will also stop playback or recording, if either are active.
         */
        void erase();

        /**
         * @brief Checks if the object is playing back recorded data.
         * 
         * @return True if playing back, else false if stopped or recording.
         */
        bool isPlaying();

        /**
         * @brief Checks if the object is recording new data.
         * 
         * @return True if recording, else false if stopped or playing back.
         */
        bool isRecording();

        /**
         * @brief Checks if the object is stopped
         * 
         * @return True if stopped, else false if recording or playing back. 
         */
        bool isStopped();

        virtual float getSampleRate();

    };

}

#endif

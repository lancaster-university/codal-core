#ifndef RECORDING_STREAM_H
#define RECORDING_STREAM_H

#include "ManagedBuffer.h"
#include "DataStream.h"

// Pretty much the largest sensible number we can have on a Micro:bit v2
#ifndef CODAL_DEFAULT_STREAM_RECORDING_MAX_LENGTH
    #define CODAL_DEFAULT_STREAM_RECORDING_MAX_LENGTH      51200
#endif

#ifndef CODAL_STREAM_RECORDING_BUFFER_SIZE
    #define CODAL_STREAM_RECORDING_BUFFER_SIZE             256
#endif

#define CODAL_STREAM_RECORDING_SIZE                        (CODAL_DEFAULT_STREAM_RECORDING_MAX_LENGTH / CODAL_STREAM_RECORDING_BUFFER_SIZE)

#define REC_STATE_STOPPED   0
#define REC_STATE_PLAYING   1
#define REC_STATE_RECORDING 2

namespace codal
{
    class StreamRecording : public DataSourceSink
    {
        private:
        uint32_t totalBufferLength;                         // Amount of data currently stored in this object.
        int state;                                          // STOPPED/PLAYING/RECORDING.
        ManagedBuffer data[CODAL_STREAM_RECORDING_SIZE];    // Buffers of data, each CODAL_STREAM_RECORDING_BUFFER_SIZE in length.
        int readOffset;                                     // Index into the buffer, indicating the current point for playback.
        int writeOffset;                                    // Index into the buffer, indicating the current point for recording.
        FiberLock recordLock, playLock;                     // Indicates to synchronous recording threads when recording/playback is complete.

        public:
        /**
         * @brief Construct a new Stream Recording object
         * 
         * @param source An upstream DataSource to connect to
         * @param length The maximum amount of memory (RAM) in bytes to allow this recording object to use. Defaults to CODAL_DEFAULT_STREAM_RECORDING_MAX_LENGTH.
         */
        StreamRecording( DataSource &source);

        virtual ManagedBuffer pull();
        virtual int pullRequest();

        /**
         * @brief Calculate and return the length <b>in bytes</b> that this StreamRecording represents
         * @return int The length, in bytes.
         */
        int length();

        /**
         * @brief Calculate the recorded duration for this StreamRecording.
         * 
         * As this cannot be known by this class (as the sample rate may change during playback <i>or</i> recording) the expected rate must be supplied.
         * 
         * @param sampleRate The sample rate to calculate the duration for, in samples per second.
         * @return The total duration of this StreamRecording, based on the supplied sample rate, in seconds.
         */
        float duration( unsigned int sampleRate );

        /**
         * @brief Begin recording data from the connected upstream
         * 
         * The StreamRecording object is not already recording, it will stop any existing playback, erase its buffer, and start recording.
         * 
         * Non-blocking, will return immediately.
         * 
         * @return Returns DEVICE_OK on completion.
         */
        int recordAsync();

        /**
         * @brief Begin recording data from the connected upstream
         * 
         * The StreamRecording object is not already recording, it will stop any existing playback, erase its buffer, and start recording.
         * 
         * Blocking call, will deschedule the current fiber until the recording completes.
         */
        void record();

        /**
         * @brief Begin playing data from the connected upstream
         * 
         * The StreamRecording object will, if already recording; stop recording, rewind to the start of its buffer, and start playing.
         * 
         * Non-blocking, will return immediately.
         * 
         * @return Returns DEVICE_OK on completion.
         */
        int playAsync();

        /**
         * @brief Begin playing data from the connected upstream
         * 
         * The StreamRecording object will, if already recording; stop recording, rewind to the start of its buffer, and start playing.
         * 
         * Blocking call, will deschedule the current fiber until the playback completes.
         */
        void play();

        /**
         * @brief Stop recording or playing the data stored in this StreamRecording object.
         * 
         * Repeated calls to this will do nothing if the object is not in a recording or playback state.
         * 
         * @return Do not use this value, return semantics are changing.
         */
        int stop();

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

        virtual void dataWanted(int wanted) override;

    };

}

#endif

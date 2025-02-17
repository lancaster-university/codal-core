#ifndef STREAM_ANALYZER_H
#define STREAM_ANALYZER_H

#if CODAL_DEBUG > 0

#include "ManagedBuffer.h"
#include "DataStream.h"

#ifndef CONFIG_STREAM_ANALYZER_DEFAULT_QUANTIZATION
#define CONFIG_STREAM_ANALYZER_DEFAULT_QUANTIZATION 32
#endif

namespace codal
{
    class StreamAnalyzer : public DataSourceSink
    {
        private:
        int             quantizationLevels;             // Number of levels requested
        int             sampleRange;                    // The maximum value of the input samples
        int             mask;                           // Value to be and'ed with each sample before analysis
        int             pullRequests;                   // Number of active pull requests
        ManagedBuffer   quantizationData;               // Long running data collected from the data stream
        ManagedBuffer   buffer;                         // The most recent buffer received

        public:
        /**
         * @brief Construct a new Stream Recording object
         * 
         * @param source An upstream DataSource to connect to
         * @param length The maximum amount of memory (RAM) in bytes to allow this recording object to use. Defaults to CODAL_DEFAULT_STREAM_RECORDING_MAX_LENGTH.
         */
        StreamAnalyzer(DataSource &source);

        virtual ManagedBuffer pull();
        virtual int pullRequest();

        /**
         * Resets the state of this StreamAnalyzer
         */
        void reset();

        /**
         * Acquires a histogram of samples seen by this analyzer.
         * @return an array holding the histogram
         */
        ManagedBuffer getData();

        /**
         * Acquires the quanitzation level used by this analyzer.
         * @return an integer represneting the number of levels of quanitzation.
         */
        int getQuantization();

        /**
         * Defines the quanitzation level used by this analyzer.
         * @return DEVICE_OK on succes, or DEVICE_INVALID_PARAMETER
         */
        int setQuantization(int quantization);

        /**
         * Acquires the sample range used by this analyzer.
         * @return an integer representing the sample range.
         */
        int getRange();

        /**
         * Defines the sample range used by this analyzer.
         * @param range A positive integer value defining the maximum allowable value.
         */
        void setRange(int range);

        /** 
         * Defines a value that is and'ed with each sample before analysis
         * Useful to mask off unwanted bit in the sample data.
         */
        void setAndMask(int mask);
    };

}

#endif
#endif

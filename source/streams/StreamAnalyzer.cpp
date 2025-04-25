#include "StreamAnalyzer.h"
#include "ErrorNo.h"
#include "DataStream.h"
#include "ManagedBuffer.h"
#include "CodalDmesg.h"

#if CODAL_DEBUG > 0

using namespace codal;

StreamAnalyzer::StreamAnalyzer(DataSource &source) : DataSourceSink( source )
{   
    this->setQuantization(CONFIG_STREAM_ANALYZER_DEFAULT_QUANTIZATION);
    this->setAndMask(0);
    this->pullRequests = 0;
}

ManagedBuffer StreamAnalyzer::pull()
{
    return buffer;
}

int StreamAnalyzer::pullRequest()
{
    // Our upstream may issue a pullRequest() to use in response to a pull().
    // Prevent recursive loops, and serialize the order of packets.
    if (pullRequests++)
        return DEVICE_OK;

    while (pullRequests)
    {
        buffer = upStream.pull();
        pullRequests--;

        //DMESG("buffer: %p", buffer.getBytes());

        // Update our recorded data.
        if (upStream.getFormat() == DATASTREAM_FORMAT_8BIT_UNSIGNED)
        {
            for (int i=0; i<buffer.length(); i++)
            {
                int s = ((int)buffer[i]);
                s &= mask;
                s = s * quantizationLevels / sampleRange;
                uint32_t *value = (uint32_t *) &quantizationData[s*4];
                (*value)++;
            }
        }

        if (upStream.getFormat() == DATASTREAM_FORMAT_8BIT_SIGNED)
        {
            for (int i=0; i<buffer.length(); i++)
            {
                int s = ((int) ((int8_t)buffer[i]));
                s &= mask;
                s = s + 128;
                s = s * quantizationLevels / sampleRange;

                uint32_t *value = (uint32_t *) &quantizationData[s*4];
                (*value)++;
            }
        }

        if (upStream.getFormat() == DATASTREAM_FORMAT_16BIT_UNSIGNED)
        {
            for (int i=0; i<buffer.length(); i+=2)
            {
                int s = (int)(*((uint16_t*) &buffer[i]));
                s &= mask;
                s = s * quantizationLevels / sampleRange;
                uint32_t *value = (uint32_t *) &quantizationData[s*4];
                (*value)++;
            }
        }

        if (upStream.getFormat() == DATASTREAM_FORMAT_16BIT_SIGNED)
        {
            for (int i=0; i<buffer.length(); i+=2)
            {
                int s = ((int) ((int16_t)buffer[i]));
                s &= mask;
                s = s + 32768;
                s = s * quantizationLevels / sampleRange;
                uint32_t *value = (uint32_t *) &quantizationData[s*4];
                (*value)++;
            }
        }

        if (downStream)
            downStream->pullRequest();
    }

    return DEVICE_OK;
}

/**
 * Resets the state of this StreamAnalyzer
 */
void StreamAnalyzer::reset()
{
    quantizationData.fill(0);
}

/**
 * Acquires a histogram of samples seen by this analyzer.
 * @return an array holding the histogram
 */
ManagedBuffer StreamAnalyzer::getData()
{
    return quantizationData;
}

/**
 * Acquires the quanitzation level used by this analyzer.
 * @return an integer represneting the number of levels of quanitzation.
 */
int StreamAnalyzer::getQuantization()
{
    return quantizationLevels;
}

/**
 * Defines the quanitzation level used by this analyzer.
 */
int StreamAnalyzer::setQuantization(int quantization)
{
    if (quantization <= 0)
        return DEVICE_INVALID_PARAMETER;

    quantizationLevels = quantization;

    // Allocate sufficent memory to hold the quantiatio buffer
    // We add one here as the fast-path maths can be inclusive when presented with a 
    // maximum sample. It's easier to accomdate it than to remove it.
    quantizationData = ManagedBuffer((1+quantizationLevels)*4);

    DMESG("Quantizations levels to: %d", quantizationLevels);

    return DEVICE_OK;
}

/**
 * Acquires the sample range used by this analyzer.
 * @return an integer representing the sample range.
 */
int StreamAnalyzer::getRange()
{
    return sampleRange;
}

/**
 * Defines the sample range used by this analyzer.
 * @param range A positive integer value defining the maximum allowable value.
 */
void StreamAnalyzer::setRange(int range)
{
    sampleRange = range;
    DMESG("SampleRange set to: %d", sampleRange);
}

/** 
 * Defines a value that is and'ed with each sample before analysis
 * Useful to mask off unwanted bit in the sample data.
 */
void StreamAnalyzer::setAndMask(int mask)
{
    this->mask = ~mask;
}

#endif
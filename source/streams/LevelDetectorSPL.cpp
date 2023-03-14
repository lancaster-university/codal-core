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

#include "CodalConfig.h"
#include "Event.h"
#include "CodalCompat.h"
#include "Timer.h"
#include "LevelDetector.h"
#include "LevelDetectorSPL.h"
#include "ErrorNo.h"
#include "StreamNormalizer.h"
#include "CodalDmesg.h"

using namespace codal;

LevelDetectorSPL::LevelDetectorSPL(DataSource &source, float highThreshold, float lowThreshold, float gain, float minValue, uint16_t id, bool connectImmediately) : upstream(source)
{
    this->id = id;
    this->level = 0;
    this->windowSize = LEVEL_DETECTOR_SPL_DEFAULT_WINDOW_SIZE;
    this->lowThreshold = lowThreshold;
    this->highThreshold = highThreshold;
    this->minValue = minValue;
    this->gain = gain;
    this->status |= LEVEL_DETECTOR_SPL_INITIALISED;
    this->unit = LEVEL_DETECTOR_SPL_DB;
    enabled = true;
    if(connectImmediately){
        upstream.connect(*this);
        this->activated = true;
    }
    else{
        this->activated = false;
    }

    this->quietBlockCount = 0;
    this->noisyBlockCount = 0;
    this->inNoisyBlock = false;
    this->maxRms = 0;
}

/**
 * Callback provided when data is ready.
 */
int LevelDetectorSPL::pullRequest()
{
    ManagedBuffer b = upstream.pull();

    uint8_t *data = &b[0];
    
    int format = upstream.getFormat();
    int skip = 1;
    float multiplier = 256;
    windowSize = 256;

    if (format == DATASTREAM_FORMAT_16BIT_SIGNED || format == DATASTREAM_FORMAT_UNKNOWN){
        skip = 2;
        multiplier = 1;
        windowSize = 128;
    }
    else if (format == DATASTREAM_FORMAT_32BIT_SIGNED){
        skip = 4;
        windowSize = 64;
        multiplier = (1/65536);
    }

    int samples = b.length() / skip;

    while(samples){
        // ensure we use at least windowSize number of samples (128)
        if(samples < windowSize)
        break;

        uint8_t *ptr, *end;

        ptr = data;
        end = data + windowSize;

        float pref = 0.00002;

        /*******************************
        *   GET MAX VALUE
        ******************************/
        int16_t maxVal = 0;
        int16_t minVal = 32766;
        int32_t v;
        ptr = data;
        while (ptr < end) {
            v = (int32_t) StreamNormalizer::readSample[format](ptr);
            if (v > maxVal) maxVal = v;
            if (v < minVal) minVal = v;
            ptr += skip;
        }
        maxVal = (maxVal - minVal) / 2;

        /*******************************
        *   GET RMS AMPLITUDE FOR CLAP DETECTION
        ******************************/
        int sumSquares = 0;
        int count = 0;
        ptr = data;
        while (ptr < end) {
            count++;
            v = (int32_t) StreamNormalizer::readSample[format](ptr) - minVal;   // need to sub minVal to avoid overflow
            sumSquares += v * v;
            ptr += skip;
        }
        float rms = sqrt(sumSquares / count);

        /*******************************
        *   CALCULATE SPL
        ******************************/
        float conv = ((float) maxVal * multiplier) / ((1 << 15) - 1) * gain;
        conv = 20 * log10(conv / pref);

        if (conv < minValue) level = minValue;
        else if (isfinite(conv)) level = conv;
        else level = minValue;

        samples -= windowSize;
        data += windowSize;

        /*******************************
        *   EMIT EVENTS
        ******************************/

        // HIGH THRESHOLD
        if ((!(status & LEVEL_DETECTOR_SPL_HIGH_THRESHOLD_PASSED)) && level > highThreshold)
        {
            Event(id, LEVEL_THRESHOLD_HIGH);
            status |=  LEVEL_DETECTOR_SPL_HIGH_THRESHOLD_PASSED;
            status &= ~LEVEL_DETECTOR_SPL_LOW_THRESHOLD_PASSED;
        }

        // LOW THRESHOLD
        if ((!(status & LEVEL_DETECTOR_SPL_LOW_THRESHOLD_PASSED)) && level < lowThreshold)
        {
            Event(id, LEVEL_THRESHOLD_LOW);
            status |=  LEVEL_DETECTOR_SPL_LOW_THRESHOLD_PASSED;
            status &= ~LEVEL_DETECTOR_SPL_HIGH_THRESHOLD_PASSED;
        }

        // CLAP DETECTION HANDLING
        if (this->inNoisyBlock && rms > this->maxRms) this->maxRms = rms;

        if (
                (       // if start of clap
                        !this->inNoisyBlock &&
                        rms > LEVEL_DETECTOR_SPL_BEGIN_POSS_CLAP_RMS &&
                        this->quietBlockCount >= LEVEL_DETECTOR_SPL_CLAP_MIN_QUIET_BLOCKS
                ) ||
                (       // or if continuing a clap
                        this->inNoisyBlock &&
                        rms > LEVEL_DETECTOR_SPL_CLAP_OVER_RMS
                )) {
            // noisy block
            if (!this->inNoisyBlock) this->maxRms = rms;
            this->quietBlockCount = 0;
            this->noisyBlockCount += 1;
            this->inNoisyBlock = true;

        } else {
            // quiet block
            if (    // if not too long, not too short, and loud enough
                    this->noisyBlockCount <= LEVEL_DETECTOR_SPL_CLAP_MAX_LOUD_BLOCKS &&
                    this->noisyBlockCount >= LEVEL_DETECTOR_SPL_CLAP_MIN_LOUD_BLOCKS &&
                    this->maxRms >= LEVEL_DETECTOR_SPL_MIN_IN_CLAP_RMS
                    ) {
                Event(id, LEVEL_DETECTOR_SPL_CLAP);
            }
            this->inNoisyBlock = false;
            this->noisyBlockCount = 0;
            this->quietBlockCount += 1;
            this->maxRms = 0;
        }
    }

    return DEVICE_OK;
}

/*
 * Determines the instantaneous value of the sensor, in SI units, and returns it.
 *
 * @return The current value of the sensor.
 */
float LevelDetectorSPL::getValue()
{
    if(!activated){
        // Register with our upstream component: on demand activated
        upstream.connect(*this);
        activated = true;
    }

    return splToUnit(level);
}

/*
 * Disable / turn off this level detector
 *
 */
void LevelDetectorSPL::disable(){
    enabled = false;
}


/**
 * Set threshold to the given value. Events will be generated when these thresholds are crossed.
 *
 * @param value the LOW threshold at which a LEVEL_THRESHOLD_LOW will be generated.
 *
 * @return DEVICE_OK on success, DEVICE_INVALID_PARAMETER if the request fails.
 */
int LevelDetectorSPL::setLowThreshold(float value)
{
    // Convert specified unit into db if necessary
    value = unitToSpl(value);

    // Protect against churn if the same threshold is set repeatedly.
    if (lowThreshold == value)
        return DEVICE_OK;

    // We need to update our threshold
    lowThreshold = value;

    // Reset any exisiting threshold state, and enable threshold detection.
    status &= ~LEVEL_DETECTOR_SPL_LOW_THRESHOLD_PASSED;

    // If a HIGH threshold has been set, ensure it's above the LOW threshold.
    if (highThreshold < lowThreshold)
        setHighThreshold(lowThreshold+1);

    return DEVICE_OK;
}

/**
 * Set threshold to the given value. Events will be generated when these thresholds are crossed.
 *
 * @param value the HIGH threshold at which a LEVEL_THRESHOLD_HIGH will be generated.
 *
 * @return DEVICE_OK on success, DEVICE_INVALID_PARAMETER if the request fails.
 */
int LevelDetectorSPL::setHighThreshold(float value)
{
    // Convert specified unit into db if necessary
    value = unitToSpl(value);

    // Protect against churn if the same threshold is set repeatedly.
    if (highThreshold == value)
        return DEVICE_OK;

    // We need to update our threshold
    highThreshold = value;

    // Reset any exisiting threshold state, and enable threshold detection.
    status &= ~LEVEL_DETECTOR_SPL_HIGH_THRESHOLD_PASSED;

    // If a HIGH threshold has been set, ensure it's above the LOW threshold.
    if (lowThreshold > highThreshold)
        setLowThreshold(highThreshold - 1);

    return DEVICE_OK;
}

/**
 * Determines the currently defined low threshold.
 *
 * @return The current low threshold. DEVICE_INVALID_PARAMETER if no threshold has been defined.
 */
float LevelDetectorSPL::getLowThreshold()
{
    return splToUnit(lowThreshold);
}

/**
 * Determines the currently defined high threshold.
 *
 * @return The current high threshold. DEVICE_INVALID_PARAMETER if no threshold has been defined.
 */
float LevelDetectorSPL::getHighThreshold()
{
    return splToUnit(highThreshold);
}

/**
 * Set the window size to the given value. The window size defines the number of samples used to determine a sound level.
 * The higher the value, the more accurate the result will be. The lower the value, the more responsive the result will be.
 * Adjust this value to suit the requirements of your applicaiton.
 *
 * @param size The size of the window to use (number of samples).
 *
 * @return DEVICE_OK on success, DEVICE_INVALID_PARAMETER if the request fails.
 */
int LevelDetectorSPL::setWindowSize(int size)
{
    if (size <= 0)
        return DEVICE_INVALID_PARAMETER;

    this->windowSize = size;
    return DEVICE_OK;
}

int LevelDetectorSPL::setGain(float gain)
{
    this->gain = gain;
    return DEVICE_OK;
}

/**
 * Defines the units that will be returned by the getValue() function.
 *
 * @param unit Either LEVEL_DETECTOR_SPL_DB or LEVEL_DETECTOR_SPL_8BIT.
 * @return DEVICE_OK or DEVICE_INVALID_PARAMETER.
 */
int LevelDetectorSPL::setUnit(int unit)
{
    if (unit == LEVEL_DETECTOR_SPL_DB || unit == LEVEL_DETECTOR_SPL_8BIT)
    {
        this->unit = unit;
        return DEVICE_OK;
    }

    return DEVICE_INVALID_PARAMETER;
}


float LevelDetectorSPL::splToUnit(float level)
{
    if (unit == LEVEL_DETECTOR_SPL_8BIT)
    {
        level = (level - LEVEL_DETECTOR_SPL_8BIT_000_POINT) * LEVEL_DETECTOR_SPL_8BIT_CONVERSION;

        // Ensure the result is clamped into the expected range.
        if (level < 0.0f)
            level = 0.0f;

        if (level > 255.0f)
            level = 255.0f;
    }

    return level;
}


float LevelDetectorSPL::unitToSpl(float level)
{
    if (unit == LEVEL_DETECTOR_SPL_8BIT)
        level = LEVEL_DETECTOR_SPL_8BIT_000_POINT + level / LEVEL_DETECTOR_SPL_8BIT_CONVERSION;

    return level;
}

/**
 * Destructor.
 */
LevelDetectorSPL::~LevelDetectorSPL()
{
}

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
#include "CodalAssert.h"

using namespace codal;

LevelDetectorSPL::LevelDetectorSPL(DataSource &source, float highThreshold, float lowThreshold, float gain, float minValue, uint16_t id) : upstream(source), resourceLock(0)
{
    this->id = id;
    this->level = 0;
    this->windowSize = LEVEL_DETECTOR_SPL_DEFAULT_WINDOW_SIZE;
    this->lowThreshold = lowThreshold;
    this->highThreshold = highThreshold;
    this->minValue = minValue;
    this->gain = gain;
    this->status |= LEVEL_DETECTOR_SPL_INITIALISED | LEVEL_DETECTOR_SPL_DATA_REQUESTED;
    this->unit = LEVEL_DETECTOR_SPL_DB;
    this->enabled = true;

    this->quietBlockCount = 0;
    this->noisyBlockCount = 0;
    this->inNoisyBlock = false;
    this->maxRms = 0;

    this->bufferCount = 0;
    this->listenerCount = 0;

    // Request a periodic callback
    status |= DEVICE_COMPONENT_STATUS_SYSTEM_TICK;

    source.connect(*this);
}


/**
  * Periodic callback from Device system timer.
  * Change the upstream active status accordingly.
  */
void LevelDetectorSPL::periodicCallback()
{
    // Ensure we don't timeout whilst waiting for data to stabilise.
    if (this->bufferCount < LEVEL_DETECTOR_SPL_MIN_BUFFERS)
    {
        //DMESG("ALLOWING BUFFERS TO FILL...");
        return;
    }

    // Calculate the time since the last request for data.
    // If this is above the given threshold and our channel is active, request that the upstream generation of data be stopped.
    if (status & LEVEL_DETECTOR_SPL_DATA_REQUESTED && !listenerCount && resourceLock.getWaitCount() == 0 && (system_timer->getTime() - this->timestamp >= LEVEL_DETECTOR_SPL_TIMEOUT))
    {
        //DMESG("LevelDetectorSPL: CALLBACK: DATA NO LONGER REQUIRED...");
        this->status &= ~LEVEL_DETECTOR_SPL_DATA_REQUESTED;
        upstream.dataWanted(DATASTREAM_NOT_WANTED);

        // Set the buffercount to just below the threshold,such that any calling fibers will block
        // until data is available, but we won't wait too long...
        this->bufferCount = LEVEL_DETECTOR_SPL_MIN_BUFFERS - 1;
    }
}

int LevelDetectorSPL::pullRequest()
{
    //DMESG("LevelDetectorSPL: PR");

    // Ignore the first LEVEL_DETECTOR_SPL_MIN_BUFFERS buffers, as we wait for the microphone to level out
    if( this->bufferCount < LEVEL_DETECTOR_SPL_MIN_BUFFERS ) {
        this->bufferCount++;
        return DEVICE_OK;
    }

    // Wake any sleeping fibers waiting for this component to start
    if( this->resourceLock.getWaitCount() > 0 )
        this->resourceLock.notifyAll();

    // If we haven't requested data and there are no active listeners, there's nothing to do.
    if (!(status & LEVEL_DETECTOR_SPL_DATA_REQUESTED || listenerCount))
    {
        //DMESG("LevelDetectorSPL: PR: ignoring data");
        return DEVICE_OK;
    }

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
        float rms = sqrtf(sumSquares / count);

        /*******************************
        *   CALCULATE SPL
        ******************************/
        float conv = ((float) maxVal * multiplier) / ((1 << 15) - 1) * gain;
        conv = 20.0f * log10f(conv / pref);

        if (conv < minValue)
            level = minValue;
        else if (isfinite(conv))
            level = conv;
        else
            level = minValue;

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
        else if ((!(status & LEVEL_DETECTOR_SPL_LOW_THRESHOLD_PASSED)) && level < lowThreshold)
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
            if (!this->inNoisyBlock)
                this->maxRms = rms;
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

float LevelDetectorSPL::getValue( int scale )
{
    // Update out timestamp.
    this->timestamp = system_timer->getTime();

    if (!(status & LEVEL_DETECTOR_SPL_DATA_REQUESTED))
    {
        // We've just been asked for data after a (potentially) long wait.
        // Let our upstream components know that we're interested in data again.
        //DMESG("LevelDetectorSPL: getValue: dataWanted(1)");
        status |= LEVEL_DETECTOR_SPL_DATA_REQUESTED;
        upstream.dataWanted(DATASTREAM_WANTED);
    }

    // Lock the resource, THEN bump the timout, so we get consistent on-time
    if(this->bufferCount < LEVEL_DETECTOR_SPL_MIN_BUFFERS)
    {
        //DMESG("WAITING ON LevelDetectorSPL::resourceLock...");
        //codal_dmesg_flush();
        resourceLock.wait();
        //DMESG("Escaped!");
        //codal_dmesg_flush();
    }

    return splToUnit( this->level, scale );
}

void LevelDetectorSPL::disable(){
    enabled = false;
}

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

float LevelDetectorSPL::getLowThreshold()
{
    return splToUnit(lowThreshold);
}

float LevelDetectorSPL::getHighThreshold()
{
    return splToUnit(highThreshold);
}

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

int LevelDetectorSPL::setUnit(int unit)
{
    if (unit == LEVEL_DETECTOR_SPL_DB || unit == LEVEL_DETECTOR_SPL_8BIT)
    {
        this->unit = unit;
        return DEVICE_OK;
    }

    return DEVICE_INVALID_PARAMETER;
}


float LevelDetectorSPL::splToUnit(float level, int queryUnit)
{
    queryUnit = queryUnit == -1 ? unit : queryUnit;

    if (queryUnit == LEVEL_DETECTOR_SPL_8BIT)
    {
        level = (level - (float)(LEVEL_DETECTOR_SPL_8BIT_000_POINT)) * LEVEL_DETECTOR_SPL_8BIT_CONVERSION;

        // Ensure the result is clamped into the expected range.
        if (level < 0.0f)
            level = 0.0f;

        if (level > 255.0f)
            level = 255.0f;
    }

    return level;
}


float LevelDetectorSPL::unitToSpl(float level, int queryUnit)
{
    queryUnit = queryUnit == -1 ? unit : queryUnit;

    if (unit == LEVEL_DETECTOR_SPL_8BIT)
        level = (float)(LEVEL_DETECTOR_SPL_8BIT_000_POINT) + level / LEVEL_DETECTOR_SPL_8BIT_CONVERSION;

    return level;
}

void LevelDetectorSPL::listenerAdded()
{
    this->listenerCount++;
    this->getValue();
}

void LevelDetectorSPL::listenerRemoved()
{
    this->listenerCount--;
}


LevelDetectorSPL::~LevelDetectorSPL()
{
}

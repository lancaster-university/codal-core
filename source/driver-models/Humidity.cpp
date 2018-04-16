/*
The MIT License (MIT)

Copyright (c) 2017 Lancaster University.
Copyright (c) 2018 Paul ADAM, Europe.

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

#include "Humidity.h"
#include "ErrorNo.h"
#include "Event.h"
#include "CodalCompat.h"
#include "CodalFiber.h"

using namespace codal;


/**
  * Constructor.
  * Create a software abstraction of a humidity sensor
  *
  * @param _i2c an instance of I2C used to communicate with the device.
  *
  * @param address the default I2C address of the humidity.
  *
 */
Humidity::Humidity(uint16_t id) : sample()
{
    // Store our identifiers.
    this->id = id;
    this->status = 0;

    // Set a default rate of 10Hz.
    this->samplePeriod = 100;
    this->sampleRange = 1;
}

/**
  * Stores data from the humidity sensor in our buffer.
  *
  * On first use, this member function will attempt to add this component to the
  * list of fiber components in order to constantly update the values stored
  * by this object.
  *
  * This lazy instantiation means that we do not
  * obtain the overhead from non-chalantly adding this component to fiber components.
  *
  * @return DEVICE_OK on success, DEVICE_I2C_ERROR if the read request fails.
  */
int Humidity::update(uint16_t s)
{
    // Indicate that pitch and roll data is now stale, and needs to be recalculated if needed.
    status &= ~TEMPERATURE_IMU_DATA_VALID;

    // Indicate that a new sample is available
    Event e(id, TEMPERATURE_EVT_DATA_UPDATE);

    return DEVICE_OK;
};

/**
  * Attempts to set the sample rate of the humidity to the specified value (in ms).
  *
  * @param period the requested time between samples, in milliseconds.
  *
  * @return DEVICE_OK on success, DEVICE_I2C_ERROR is the request fails.
  *
  * @code
  * // sample rate is now 20 ms.
  * humidity.setPeriod(20);
  * @endcode
  *
  * @note The requested rate may not be possible on the hardware. In this case, the
  * nearest lower rate is chosen.
  */
int Humidity::setPeriod(int period)
{
    int result;

    samplePeriod = period;
    result = configure();

    samplePeriod = getPeriod();
    return result;

}

/**
  * Reads the currently configured sample rate of the humidity.
  *
  * @return The time between samples, in milliseconds.
  */
int Humidity::getPeriod()
{
    return (int)samplePeriod;
}

/**
  * Attempts to set the sample range of the humidity to the specified value.
  *
  * @param range The requested sample range of samples.
  *
  * @return DEVICE_OK on success, DEVICE_I2C_ERROR is the request fails.
  *
  * @code
  * // the sample range of the humidity is now.
  * humidity.setRange(8);
  * @endcode
  *
  * @note The requested range may not be possible on the hardware. In this case, the
  * nearest lower range is chosen.
  */
int Humidity::setRange(int range)
{
    int result;

    sampleRange = range;
    result = configure();

    sampleRange = getRange();
    return result;
}

/**
  * Reads the currently configured sample range of the humidity.
  *
  * @return The sample range.
  */
int Humidity::getRange()
{
    return (int)sampleRange;
}

/**
 * Configures the humidity and sample rate defined
 * in this object. The nearest values are chosen to those defined
 * that are supported by the hardware. The instance variables are then
 * updated to reflect reality.
 *
 * @return DEVICE_OK on success, DEVICE_I2C_ERROR if the humidity could not be configured.
 *
 * @note This method should be overidden by the hardware driver to implement the requested
 * changes in hardware.
 */
int Humidity::configure()
{
    return DEVICE_NOT_SUPPORTED;
}

/**
 * Poll to see if new data is available from the hardware. If so, update it.
 * n.b. it is not necessary to explicitly call this function to update data
 * (it normally happens in the background when the scheduler is idle), but a check is performed
 * if the user explicitly requests up to date data.
 *
 * @return DEVICE_OK on success, DEVICE_I2C_ERROR if the update fails.
 *
 * @note This method should be overidden by the hardware driver to implement the requested
 * changes in hardware.
 */
int Humidity::requestUpdate()
{
    return DEVICE_NOT_SUPPORTED;
}

/**
 * Reads the last humidity value stored.
 * @return The humidity measured.
 */
uint16_t Humidity::getSample()
{
    requestUpdate();
    return sample;
}

/**
  * Destructor, where we deregister from the array of fiber components.
  */
Humidity::~Humidity()
{
}


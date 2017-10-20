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

/**
 * Class definition for an FXS8700 3 axis accelerometer.
 *
 * Represents an implementation of the Freescale FXS8700 3 axis accelerometer
 * Also includes basic data caching and on demand activation.
 */
#include "CodalConfig.h"
#include "CodalDmesg.h"
#include "FXOS8700Accelerometer.h"
#include "ErrorNo.h"
#include "Event.h"
#include "CodalCompat.h"
#include "CodalFiber.h"

using namespace codal;

/**
 * Constructor.
 * Create a software abstraction of an accelerometer.
 *
 * @param fxos8700 A reference to the hardware driver for the FXOS8700 hybrid accelerometer/magnetometer.
 * @param coordinateSpace The orientation of the sensor. Defaults to: SIMPLE_CARTESIAN
 * @param id The unique EventModel id of this component. Defaults to: DEVICE_ID_ACCELEROMETER
 *
 */
FXOS8700Accelerometer::FXOS8700Accelerometer(FXOS8700 &fxos8700, CoordinateSpace &coordinateSpace, uint16_t id) : Accelerometer(coordinateSpace, id), fxo(fxos8700)
{
    fxo.setAccelerometerAPI(this);
}

/**
 * Reads the currently configured sample rate of the accelerometer.
 *
 * @return The time between samples, in milliseconds.
 */
int FXOS8700Accelerometer::getPeriod()
{
    return fxo.getPeriod();
}

/**
 * Reads the currently configured sample range of the accelerometer.
 *
 * @return The sample range, in g.
 */
int FXOS8700Accelerometer::getRange()
{
    return fxo.getRange();
}


/**
 * Configures the accelerometer for G range and sample rate defined
 * in this object. The nearest values are chosen to those defined
 * that are supported by the hardware. The instance variables are then
 * updated to reflect reality.
 *
 * @return DEVICE_OK on success, DEVICE_I2C_ERROR if the accelerometer could not be configured.
 *
 * @note This method should be overidden by the hardware driver to implement the requested
 * changes in hardware.
 */
int FXOS8700Accelerometer::configure()
{
    int result;

    // First try to update the range of the sensor
    result = fxo.setRange(sampleRange);
    if (result != DEVICE_OK)
        return result;

    // Now the sample rate
    result = fxo.setPeriod(samplePeriod);
    if (result != DEVICE_OK)
        return result;

    return DEVICE_OK;
}

/**
 * Poll to see if new data is available from the hardware. If so, update it.
 * n.b. it is not necessary to explicitly call this funciton to update data
 * (it normally happens in the background when the scheduler is idle), but a check is performed
 * if the user explicitly requests up to date data.
 *
 * @return DEVICE_OK on success, DEVICE_I2C_ERROR if the update fails.
 *
 * @note This method should be overidden by the hardware driver to implement the requested
 * changes in hardware.
 */
int FXOS8700Accelerometer::requestUpdate()
{
    fxo.updateSample();
    return DEVICE_OK;
}

/*
 * Callback function, invoked whenever new data is available from the hardware.
 */
void FXOS8700Accelerometer::dataReady()
{
    // Simply read the data from the hardwae driver, and inject it into the higher level accelerometer buffer.
    update(fxo.getAccelerometerSample());
}

/**
 * Destructor.
 */
FXOS8700Accelerometer::~FXOS8700Accelerometer()
{
}


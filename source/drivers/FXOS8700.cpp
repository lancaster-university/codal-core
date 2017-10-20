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
#include "FXOS8700.h"
#include "FXOS8700Accelerometer.h"
#include "FXOS8700Magnetometer.h"
#include "ErrorNo.h"
#include "Event.h"
#include "CodalCompat.h"
#include "CodalFiber.h"
#include "CodalDmesg.h"
#include "Accelerometer.h"

using namespace codal;

//
// Configuration table for available g force ranges.
// Maps g -> XYZ_DATA_CFG bit [0..1]
//
static const KeyValueTableEntry accelerometerRangeData[] = {
    {2,0},
    {4,1},
    {8,2}
};
CREATE_KEY_VALUE_TABLE(accelerometerRange, accelerometerRangeData);

//
// Configuration table for available data update frequency.
// maps microsecond period -> CTRL_REG1 data rate selection bits [3..5]
//
static const KeyValueTableEntry accelerometerPeriodData[] = {
    {2500,0x00},
    {5000,0x08},
    {10000,0x10},
    {20000,0x18},
    {80000,0x20},
    {160000,0x28},
    {320000,0x30},
    {1280000,0x38}
};
CREATE_KEY_VALUE_TABLE(accelerometerPeriod, accelerometerPeriodData);


/**
  * Configures the accelerometer for G range and sample rate defined
  * in this object. The nearest values are chosen to those defined
  * that are supported by the hardware. The instance variables are then
  * updated to reflect reality.
  *
  * @return DEVICE_OK on success, DEVICE_I2C_ERROR if the accelerometer could not be configured.
  */
int FXOS8700::configure()
{
    int result;
    uint8_t value;

    // First find the nearest sample rate to that specified.
    samplePeriod = accelerometerPeriod.getKey(samplePeriod * 1000) / 1000;
    sampleRange = accelerometerRange.getKey(sampleRange);

    // Now configure the accelerometer accordingly.

    // Firstly, disable the module (as some registers cannot be changed while its running).
    value = 0x00;
    result = i2c.writeRegister(address, FXOS8700_CTRL_REG1, value);
    if (result != 0)
    {
        DMESG("I2C ERROR: FXOS8700_CTRL_REG1");
        return DEVICE_I2C_ERROR;
    }

    // Enter hybrid mode (interleave accelerometer and magnetometer samples).
    // Also, select full oversampling on the magnetometer
    // TODO: Determine power / accuracy tradeoff here.
    value = 0x1F;
    result = i2c.writeRegister(address, FXOS8700_M_CTRL_REG1, value);
    if (result != 0)
    {
        DMESG("I2C ERROR: FXOS8700_M_CTRL_REG1");
        return DEVICE_I2C_ERROR;
    }

    // Select the auto incremement mode, which allows a contiguous I2C block
    // read of both acceleromter and magnetometer data despite them being non-contguous
    // in memory... funky!
    value = 0x20;
    result = i2c.writeRegister(address, FXOS8700_M_CTRL_REG2, value);
    if (result != 0)
    {
        DMESG("I2C ERROR: FXOS8700_M_CTRL_REG2");
        return DEVICE_I2C_ERROR;
    }

    // Configure PushPull Active LOW interrupt mode.
    // n.b. This may need to be reconfigured if the interrupt line is shared.
    value = 0x00;
    result = i2c.writeRegister(address, FXOS8700_CTRL_REG3, value);
    if (result != 0)
    {
        DMESG("I2C ERROR: FXOS8700_CTRL_REG3");
        return DEVICE_I2C_ERROR;
    }

    // Enable a data ready interrupt.
    // TODO: This is currently PUSHPULL mode. This may nede to be reconfigured
    // to OPEN_DRAIN if the interrupt line is shared.
    value = 0x01;
    result = i2c.writeRegister(address, FXOS8700_CTRL_REG4, value);
    if (result != 0)
    {
        DMESG("I2C ERROR: FXOS8700_CTRL_REG4");
        return DEVICE_I2C_ERROR;
    }

    // Route the data ready interrupt to INT1 pin.
    value = 0x01;
    result = i2c.writeRegister(address, FXOS8700_CTRL_REG5, value);
    if (result != 0)
    {
        DMESG("I2C ERROR: FXOS8700_CTRL_REG5");
        return DEVICE_I2C_ERROR;
    }

    // Configure acceleromter g range.
    value = accelerometerRange.get(sampleRange);
    result = i2c.writeRegister(address, FXOS8700_XYZ_DATA_CFG, value);
    if (result != 0)
    {
        DMESG("I2C ERROR: FXOS8700_XYZ_DATA_CFG");
        return DEVICE_I2C_ERROR;
    }

    // Configure sample rate and re-enable the sensor.
    value = accelerometerPeriod.get(samplePeriod * 1000) | 0x01;
    result = i2c.writeRegister(address, FXOS8700_CTRL_REG1, value);
    if (result != 0)
    {
        DMESG("I2C ERROR: FXOS8700_CTRL_REG1");
        return DEVICE_I2C_ERROR;
    }

    return DEVICE_OK;
}

/**
  * Constructor.
  * Create a software abstraction of an FXSO8700 combined accelerometer/magnetometer
  *
  * @param _i2c an instance of I2C used to communicate with the device.
  *
  * @param address the default I2C address of the accelerometer. Defaults to: FXS8700_DEFAULT_ADDR.
  *
 */
FXOS8700::FXOS8700(I2C& _i2c, Pin &_int1, uint16_t address) : i2c(_i2c), int1(_int1), accelerometerSample(), magnetometerSample()
{
    // Store our identifiers.
    this->status = 0;
    this->address = address;
    this->accelerometerAPI = NULL;
    this->magnetometerAPI = NULL;

    // Update our internal state for 50Hz at +/- 2g (50Hz has a period af 20ms, which we double here as we're in "hybrid mode").
    this->samplePeriod = 40;
    this->sampleRange = 2;

    // Configure and enable the accelerometer.
    configure();
}

/**
  * Attempts to read the 8 bit ID from the accelerometer, this can be used for
  * validation purposes.
  *
  * @return the 8 bit ID returned by the accelerometer, or DEVICE_I2C_ERROR if the request fails.
  *
  * @code
  * accelerometer.whoAmI();
  * @endcode
  */
int FXOS8700::whoAmI()
{
    uint8_t data;
    int result;

    result = i2c.readRegister(address, FXOS8700_WHO_AM_I, &data, 1);
    if (result !=0)
        return DEVICE_I2C_ERROR;

    return (int)data;
}

/**
  * Reads the sensor ata from the FXSO8700, and stores it in our buffer.
  * This only happens if the device indicates that it has new data via int1.
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
int FXOS8700::updateSample()
{
    // Ensure we're scheduled to update the data periodically
    status |= DEVICE_COMPONENT_STATUS_IDLE_TICK;

    // Poll interrupt line from device (ACTIVE LOW)
    if(int1.getDigitalValue() == 0)
    {
        uint8_t data[12];
        uint8_t *ptr;
        int result;

        // Read the combined accelerometer and magnetometer data.
        result = i2c.readRegister(address, FXOS8700_OUT_X_MSB, data, 12);

        if (result !=0)
            return DEVICE_I2C_ERROR;

        // read sensor data (and translate into signed little endian)
        ptr = (uint8_t *)&accelerometerSample;
        *ptr++ = data[1];
        *ptr++ = data[0];
        *ptr++ = data[3];
        *ptr++ = data[2];
        *ptr++ = data[5];
        *ptr++ = data[4];

        ptr = (uint8_t *)&magnetometerSample;
        *ptr++ = data[7];
        *ptr++ = data[6];
        *ptr++ = data[9];
        *ptr++ = data[8];
        *ptr++ = data[11];
        *ptr++ = data[10];

        // scale the 14 bit data (packed into 16 bits) into SI units (milli-g)
        accelerometerSample.x = (accelerometerSample.x * this->sampleRange) / 32;
        accelerometerSample.y = (accelerometerSample.y * this->sampleRange) / 32;
        accelerometerSample.z = (accelerometerSample.z * this->sampleRange) / 32;

        // align to ENU coordinate system
        accelerometerSample.x = -accelerometerSample.x;
        magnetometerSample.x = -magnetometerSample.x;

        //DMESG("ENU: [AX:%d][AY:%d][AZ:%d][MX:%d][MY:%d][MZ:%d]", accelerometerSample.x, accelerometerSample.y, accelerometerSample.z, magnetometerSample.x, magnetometerSample.y, magnetometerSample.z);

        if (accelerometerAPI)
            accelerometerAPI->dataReady();

        if (magnetometerAPI)
            magnetometerAPI->dataReady();
    }

    return DEVICE_OK;
}

/**
  * Attempts to set the sample rate of the accelerometer to the specified value (in ms).
  *
  * @param period the requested time between samples, in milliseconds.
  *
  * @return DEVICE_OK on success, DEVICE_I2C_ERROR is the request fails.
  *
  * @code
  * // sample rate is now 20 ms.
  * accelerometer.setPeriod(20);
  * @endcode
  *
  * @note The requested rate may not be possible on the hardware. In this case, the
  * nearest lower rate is chosen.
  */
int FXOS8700::setPeriod(int period)
{
    samplePeriod = period;
    return configure();
}

/**
  * Reads the currently configured sample rate of the accelerometer.
  *
  * @return The time between samples, in milliseconds.
  */
int FXOS8700::getPeriod()
{
    return (int)samplePeriod;
}

/**
  * Attempts to set the sample range of the accelerometer to the specified value (in g).
  *
  * @param range The requested sample range of samples, in g.
  *
  * @return DEVICE_OK on success, DEVICE_I2C_ERROR is the request fails.
  *
  * @code
  * // the sample range of the accelerometer is now 8G.
  * accelerometer.setRange(8);
  * @endcode
  *
  * @note The requested range may not be possible on the hardware. In this case, the
  * nearest lower range is chosen.
  */
int FXOS8700::setRange(int range)
{
    sampleRange = range;
    return configure();
}

/**
  * Reads the currently configured sample range of the accelerometer.
  *
  * @return The sample range, in g.
  */
int FXOS8700::getRange()
{
    return (int)sampleRange;
}

/**
 * Reads the accelerometer data from the latest update retrieved from the accelerometer.
 * Data is provided in ENU format, relative to the device package (and makes no attempt
 * to align axes to the device).
 *
 * @return The force measured in each axis, in milli-g.
 *
 */
Sample3D FXOS8700::getAccelerometerSample()
{
    return accelerometerSample;
}

/**
 * Reads the magnetometer data from the latest update retrieved from the magnetometer.
 * Data is provided in ENU format, relative to the device package (and makes no attempt
 * to align axes to the device).
 *
 * @return The magnetic force measured in each axis, in micro-teslas.
 *
 */
Sample3D FXOS8700::getMagnetometerSample()
{
    return magnetometerSample;
}

/**
  * A periodic callback invoked by the fiber scheduler idle thread.
  *
  * Internally calls updateSample().
  */
void FXOS8700::idleCallback()
{
    updateSample();
}

/**
 * Register a higher level driver for our accelerometer functions
 *
 * @param a A pointer to an instance of the FXOS8700Accelerometer class to inform when new data is available.
 */
void FXOS8700::setAccelerometerAPI(FXOS8700Accelerometer *a)
{
    accelerometerAPI = a;
}

/**
 * Register a higher level driver for our magnetometer functions
 *
 * @param a A pointer to an instance of the FXOS8700Magnetometer class to inform when new data is available.
 */
void FXOS8700::setMagnetometerAPI(FXOS8700Magnetometer *m)
{
    magnetometerAPI = m;
}

/**
  * Destructor for FXS8700, where we deregister from the array of fiber components.
  */
FXOS8700::~FXOS8700()
{
}


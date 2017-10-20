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

#ifndef FXOS8700_MAGNETOMETER_H
#define FXOS8700_MAGNETOMETER_H

#include "CodalConfig.h"
#include "CodalComponent.h"
#include "CodalUtil.h"
#include "CoordinateSystem.h"
#include "FXOS8700.h"
#include "Compass.h"

namespace codal
{
    /**
     * Class definition for FXOS8700Magnetometer.
     * This class provides a simple wrapper between the hybrid FXOS8700 magnetometer and higher level magnetometer funcitonality.
     */
    class FXOS8700Magnetometer : public Compass
    {
        FXOS8700        &fxo;               // Our underlying hardware driver

        public:

        /**
          * Constructor.
          * Create a software abstraction of an magnetometer.
          *
          * @param fxos8700 A reference to the hardware driver for the FXOS8700 hybrid accelerometer/magnetometer.
          * @param coordinateSpace The orientation of the sensor. Defaults to: SIMPLE_CARTESIAN
          * @param id The unique EventModel id of this component. Defaults to: DEVICE_ID_ACCELEROMETER
          *
         */
        FXOS8700Magnetometer(FXOS8700 &fxos8700, CoordinateSpace &coordinateSpace, uint16_t id = DEVICE_ID_COMPASS);

        /**
         * Reads the currently configured sample rate of the magnetometer.
         *
         * @return The time between samples, in milliseconds.
         */
        virtual int getPeriod();

        /**
         * Configures the magnetometer for the sample rate defined
         * in this object. The nearest values are chosen to those defined
         * that are supported by the hardware. The instance variables are then
         * updated to reflect reality.
         *
         * @return DEVICE_OK on success, DEVICE_I2C_ERROR if the magnetometer could not be configured.
         *
         * @note This method should be overidden by the hardware driver to implement the requested
         * changes in hardware.
         */
        virtual int configure();

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
        virtual int requestUpdate();

        /*
         * Callback function, invoked whenever new data is available from the hardware.
         */
        void dataReady();

        /**
          * Destructor.
          */
        ~FXOS8700Magnetometer();

    };
}

#endif


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

#ifndef CODAL_HUMIDITY_H
#define CODAL_HUMIDITY_H

#include "CodalConfig.h"
#include "CodalComponent.h"
#include "Pin.h"
#include "CodalUtil.h"

/**
  * Status flags
  */
#define HUMIDITY_IMU_DATA_VALID               0x02

/**
  * Humidity events
  */
#define HUMIDITY_EVT_DATA_UPDATE              1

namespace codal
{

    /**
     * Class definition for Humidity.
     */
    class Humidity : public CodalComponent
    {
        protected:

        uint16_t        samplePeriod;       // The time between samples, in milliseconds.
        uint8_t         sampleRange;        // The sample range.
        uint16_t        sample;             // The last sample read.

        public:

        /**
          * Constructor.
          * Create a software abstraction of an humidity.
          *
           @param id the unique EventModel id of this component. Defaults to: DEVICE_ID_HUMIDITY
          *
         */
        Humidity(uint16_t id = DEVICE_ID_HUMIDITY);

        /**
          * Attempts to set the sample rate of the humidity to the specified value (in ms).
          *
          * @param period the requested time between samples, in milliseconds.
          * @return DEVICE_OK on success, DEVICE_I2C_ERROR is the request fails.
          *
          * @note The requested rate may not be possible on the hardware. In this case, the
          * nearest lower rate is chosen.
          *
          * @note This method should be overriden (if supported) by specific humidity device drivers.
          */
        virtual int setPeriod(int period);

        /**
          * Reads the currently configured sample rate of the humidity.
          *
          * @return The time between samples, in milliseconds.
          */
        virtual int getPeriod();

        /**
          * Attempts to set the sample range of the humidity to the specified value (in dps).
          *
          * @param range The requested sample range of samples, in dps.
          *
          * @return DEVICE_OK on success, DEVICE_I2C_ERROR is the request fails.
          *
          * @note The requested range may not be possible on the hardware. In this case, the
          * nearest lower range is chosen.
          *
          * @note This method should be overriden (if supported) by specific humidity device drivers.
          */
        virtual int setRange(int range);

        /**
          * Reads the currently configured sample range of the humidity.
          *
          * @return The sample range, in g.
          */
        virtual int getRange();

        /**
         * Configures the humidity for dps range and sample rate defined
         * in this object. The nearest values are chosen to those defined
         * that are supported by the hardware. The instance variables are then
         * updated to reflect reality.
         *
         * @return DEVICE_OK on success, DEVICE_I2C_ERROR if the humidity could not be configured.
         *
         * @note This method should be overidden by the hardware driver to implement the requested
         * changes in hardware.
         */
        virtual int configure();

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
        virtual int requestUpdate();

        /**
         * Stores data from the humidity sensor in our buffer, and perform gesture tracking.
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
        virtual int update(uint16_t s);

        /**
          * Reads the last humidity value stored, and in the coordinate system defined in the constructor.
          * @return The force measured in each axis, in dps.
          */
        uint16_t getSample();

        /**
          * Destructor.
          */
        ~Humidity();

        private:

    };
}

#endif

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

#ifndef CODAL_DISPLAY_H
#define CODAL_DISPLAY_H

#include "CodalConfig.h"
#include "Image.h"

#define DISPLAY_EVT_RENDER              3
#define DISPLAY_EVT_RENDER_START        2
#define DISPLAY_EVT_RENDER_COMPLETE     4

namespace codal
{
    //
    // Valid rotation settings.
    //
    enum DisplayRotation
    {
        DISPLAY_ROTATION_0,
        DISPLAY_ROTATION_90,
        DISPLAY_ROTATION_180,
        DISPLAY_ROTATION_270
    };

    /**
     * Class definition for an abstract Display.
     */
    class Display : public CodalComponent
    {
        protected:
        uint16_t width;
        uint16_t height;
        uint8_t  brightness;
        DisplayRotation rotation;

        public:

        // A mutable bitmap buffer being rendered to the display.
        Image image;

        /**
         * Constructor.
         *
         * Create a software representation an abstract display.
         * The display is initially blank.
         *
         * @param id The id the display should use when sending events on the MessageBus. Defaults to DEVICE_ID_DISPLAY.
         */
        Display (int width, int height, uint8_t ppb = 1, uint16_t id = DEVICE_ID_DISPLAY);

        /**
         * Returns the width of the display
         *
         * @return display width
         *
         */
        virtual int getWidth();

        /**
         * Returns the height of the display
         *
         * @return display height
         *
         */
        virtual int getHeight();

        virtual int setRotation(DisplayRotation r);
        virtual DisplayRotation getRotation();

        /**
         * Configures the brightness of the display.
         *
         * @param b The brightness to set the brightness to, in the range 0 - 255.
         *
         * @return DEVICE_OK, or DEVICE_INVALID_PARAMETER
         */
        virtual int setBrightness(int b);

        /**
         * Fetches the current brightness of this display.
         *
         * @return the brightness of this display, in the range 0..255.
         */
        virtual int getBrightness();

        /**
         * Enables the display, should only be called if the display is disabled.
         */
        virtual void enable();

        /**
         * Disables the display.
         */
        virtual void disable();

        /**
         * Captures the bitmap currently being rendered on the display.
         *
         * @return a Image containing the captured data.
         */
        virtual Image screenShot();

        /**
         * Destructor for CodalDisplay, where we deregister this instance from the array of system components.
         */
        ~Display();
    };
}

#endif

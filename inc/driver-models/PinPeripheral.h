/*
The MIT License (MIT)

Copyright (c) 2022 Lancaster University.

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

#ifndef PIN_PERIPHERAL_H
#define PIN_PERIPHERAL_H

#include "CodalConfig.h"
#include "CodalComponent.h"

namespace codal
{
    /**
      * Class definition for PinPeripheral.
      *
      * Serves as an abstract base class for any device driver that directly interacts with a Pin.
      * Provides the necessary function to enable safe, dynamic rebinding of pins to peripherals at runtime
      */
    class Pin;
    class PinPeripheral
    {
        public:
        bool deleteOnRelease = false;
        bool pinLock = false;

        /**
          * Method to release the given pin from a peripheral, if already bound.
          * Device drivers should override this method to disconnect themselves from the give pin
          * to allow it to be used by a different peripheral.
          *
          * @param pin the Pin to be released
          */
        virtual int releasePin(Pin &pin);

        /**
          * Determines if this peripheral has locked any attached pins to this peripheral.
          * During a locked period, any attempts to release or reassign those pins to a differnet peripheral are ignored.
          * This mechanism is primarily useful to use functions such as Pin::setDigitalValue() within a peripheral driver,
          * but without releasing the pin's binding to that peripheral.
          *
          * @return true if this peripherals pin bindings are locked, false otherwise.
          */
        bool isPinLocked();

        /**
          * Controls if this peripheral has locked any attached pins to this peripheral.
          * During a locked period, any attempts to release or reassign those pins to a differnet peripheral are ignored.
          * This mechanism is primarily useful to use functions such as Pin::setDigitalValue() within a peripheral driver,
          * but without releasing the pin's binding to that peripheral.
          *
          * @param true if this peripherals pin bindings are to be locked, false otherwise.
          */
        void setPinLock(bool locked);

        /**
         * Utility function, to assist in redirect() operations and consistent use of disconnect()/connect() by peripherals.
         * Safely disconnects pin from any attached peripherals, upfates pin to the new pin, and attaches to the given peripheral.
         * Also validates out NULL cases.
         *
         * @param pin Typically a mutable instance variable, holding the current pin used by a given peripheral.
         * @param newPin The pin which is replacing the value of pin.
         */
        int reassignPin(void *pin, Pin *newPin);
    };
}

#endif

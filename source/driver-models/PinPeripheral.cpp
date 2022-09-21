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

#include "Pin.h"
#include "CodalDmesg.h"

using namespace codal;

/**
 * Method to release the given pin from a peripheral, if already bound.
 * Device drivers should override this method to disconnect themselves from the give pin
 * to allow it to be used by a different peripheral.
 *
 * @param pin the Pin to be released.
 * @return DEVICE_OK on success, or DEVICE_NOT_IMPLEMENTED if unsupported, or DEVICE_INVALID_PARAMETER if the pin is not bound to this peripheral.
 */
int PinPeripheral::releasePin(Pin &pin)
{
    return DEVICE_NOT_IMPLEMENTED;
}

/**
    * Determines if this peripheral has locked any attached pins to this peripheral.
    * During a locked period, any attempts to release or reassign those pins to a differnet peripheral are ignored.
    * This mechanism is primarily useful to use functions such as Pin::setDigitalValue() within a peripheral driver,
    * but without releasing the pin's binding to that peripheral.
    *
    * @return true if this peripherals pin bindings are locked, false otherwise.
    */
bool PinPeripheral::isPinLocked()
{
    return pinLock;
}

/**
    * Controls if this peripheral has locked any attached pins to this peripheral.
    * During a locked period, any attempts to release or reassign those pins to a differnet peripheral are ignored.
    * This mechanism is primarily useful to use functions such as Pin::setDigitalValue() within a peripheral driver,
    * but without releasing the pin's binding to that peripheral.
    *
    * @param true if this peripherals pin bindings are to be locked, false otherwise.
    */
void PinPeripheral::setPinLock(bool locked)
{
    pinLock = locked;
}

/**
    * Utility function, to assist in redirect() operations and consistent use of disconnect()/connect() by peripherals.
    * Safely disconnects pin from any attached peripherals, upfates pin to the new pin, and attaches to the given peripheral.
    * Also validates out NULL cases.
    *
    * @param p Typically a mutable instance variable, holding the current pin used by a given peripheral.
    * @param newPin The pin which is replacing the value of pin.
    */
int PinPeripheral::reassignPin(void *p, Pin *newPin)
{
    Pin **pin = (Pin **)p;

    if (pin == NULL)
        return DEVICE_INVALID_PARAMETER;

    // If the pin is changing state, reelase any old peripherals and attach the new one.
    if (*pin != newPin)
    {
        if (*pin)
            (*pin)->disconnect();

        if (newPin)
            newPin->connect(*this);

        *pin = newPin;
    }

    return DEVICE_OK;
}


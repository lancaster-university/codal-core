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

#ifndef DEVICE_HID_KEYBOARD_H
#define DEVICE_HID_KEYBOARD_H

#include "HID.h"

#if CONFIG_ENABLED(DEVICE_USB)

#define HID_KEYBOARD_REPORT_GENERIC 0x01
#define HID_KEYBOARD_REPORT_CONSUMER 0x02
#define HID_KEYBOARD_KEYSTATE_SIZE_GENERIC 0x08
#define HID_KEYBOARD_KEYSTATE_SIZE_CONSUMER 0x16
#define HID_KEYBOARD_MODIFIER_OFFSET 2

namespace codal
{
    class USBHIDKeyboard : public USBHID
    {
        public:
        USBHIDKeyboard();

        int modifierKeyDown(uint8_t key, uint8_t reportID=HID_KEYBOARD_REPORT_GENERIC);
        int modifierKeyUp(uint8_t key, uint8_t reportID=HID_KEYBOARD_REPORT_GENERIC);
        int keyDown(uint8_t key, uint8_t reportID=HID_KEYBOARD_REPORT_GENERIC);
        int keyUp(uint8_t key, uint8_t reportID=HID_KEYBOARD_REPORT_GENERIC);

        uint8_t keyStateGeneric[HID_KEYBOARD_KEYSTATE_SIZE_GENERIC];
        uint8_t keyPressedCountGeneric;

        uint8_t keyStateConsumer[HID_KEYBOARD_KEYSTATE_SIZE_CONSUMER];
        uint8_t keyPressedCountConsumer;

        virtual int stdRequest(UsbEndpointIn &ctrl, USBSetup& setup);
        virtual const InterfaceInfo *getInterfaceInfo();
    };
}


#endif

#endif

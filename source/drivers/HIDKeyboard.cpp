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

#include "HIDKeyboard.h"
#include "HID.h"

#if CONFIG_ENABLED(DEVICE_USB)

using namespace codal;

//this descriptor must be stored in RAM
static char hidKeyboardDescriptor[] = {
    0x05, 0x01,                         // Usage Page (Generic Desktop)
    0x09, 0x06,                         // Usage (Keyboard)
    0xA1, 0x01,                         // Collection (Application)
    0x85, 0x01,                         //   Report ID (1)
    0x05, 0x07,                         //     Usage Page (Key Codes)
    0x19, 0xe0,                         //     Usage Minimum (224)
    0x29, 0xe7,                         //     Usage Maximum (231)
    0x15, 0x00,                         //     Logical Minimum (0)
    0x25, 0x01,                         //     Logical Maximum (1)
    0x75, 0x01,                         //     Report Size (1)
    0x95, 0x08,                         //     Report Count (8)
    0x81, 0x02,                         //     Input (Data, Variable, Absolute)

    0x95, 0x01,                         //     Report Count (1)
    0x75, 0x08,                         //     Report Size (8)
    0x81, 0x01,                         //     Input (Constant) reserved byte(1)

    0x95, 0x05,                         //     Report Count (5)
    0x75, 0x01,                         //     Report Size (1)
    0x05, 0x08,                         //     Usage Page (Page# for LEDs)
    0x19, 0x01,                         //     Usage Minimum (1)
    0x29, 0x05,                         //     Usage Maximum (5)
    0x91, 0x02,                         //     Output (Data, Variable, Absolute), Led report
    0x95, 0x01,                         //     Report Count (1)
    0x75, 0x03,                         //     Report Size (3)
    0x91, 0x01,                         //     Output (Data, Variable, Absolute), Led report padding

    0x95, 0x06,                         //     Report Count (6)
    0x75, 0x08,                         //     Report Size (8)
    0x15, 0x00,                         //     Logical Minimum (0)
    0x25, 0x65,                         //     Logical Maximum (101)
    0x05, 0x07,                         //     Usage Page (Key codes)
    0x19, 0x00,                         //     Usage Minimum (0)
    0x29, 0x65,                         //     Usage Maximum (101)
    0x81, 0x00,                         //     Input (Data, Array) Key array(6 bytes)


    0x09, 0x05,                         //     Usage (Vendor Defined)
    0x15, 0x00,                         //     Logical Minimum (0)
    0x26, 0xFF, 0x00,                   //     Logical Maximum (255)
    0x75, 0x08,                         //     Report Count (2)
    0x95, 0x02,                         //     Report Size (8 bit)
    0xB1, 0x02,                         //     Feature (Data, Variable, Absolute)

    0xC0,                                // End Collection (Application)

    0x05, 0x0c,                         // Usage Page (Consumer Devices)
    0x09, 0x01,                         // Usage (Consumer Control)
    0xa1, 0x01,                         // Collection (Application)
    0x85, 0x02,                         // Report ID (2)
    0x15, 0x00,                         // Logical Minimum (0)
    0x25, 0x01,                         // Logical Maximum (1)
    0x75, 0x01,                         // Report Size (1)
    0x95, 0x10,                         // Report Count (16)
    0x09, 0xe2,                         // Usage (Mute) 0x01
    0x09, 0xe9,                         // Usage (Volume Up) 0x02
    0x09, 0xea,                         // Usage (Volume Down) 0x03
    0x09, 0xcd,                         // Usage (Play/Pause) 0x04
    0x09, 0xb7,                         // Usage (Stop) 0x05
    0x09, 0xb6,                         // Usage (Scan Previous Track) 0x06
    0x09, 0xb5,                         // Usage (Scan Next Track) 0x07
    0x0a, 0x8a, 0x01,                   // Usage (Mail) 0x08
    0x0a, 0x92, 0x01,                   // Usage (Calculator) 0x09
    0x0a, 0x21, 0x02,                   // Usage (www search) 0x0a
    0x0a, 0x23, 0x02,                   // Usage (www home) 0x0b
    0x0a, 0x2a, 0x02,                   // Usage (www favorites) 0x0c
    0x0a, 0x27, 0x02,                   // Usage (www refresh) 0x0d
    0x0a, 0x26, 0x02,                   // Usage (www stop) 0x0e
    0x0a, 0x25, 0x02,                   // Usage (www forward) 0x0f
    0x0a, 0x24, 0x02,                   // Usage (www back) 0x10
    0x81, 0x62,                         // Input (Data,Var,Abs,NPrf,Null)
    0xc0,                               // End Collection

};

static const HIDReportDescriptor reportDesc = {
    9,
    0x21,                  // HID
    0x101,                 // hidbcd 1.01
    0x00,                  // country code
    0x01,                  // num desc
    0x22,                  // report desc type
    sizeof(hidKeyboardDescriptor),
};

static const InterfaceInfo ifaceInfo = {
    &reportDesc,
    sizeof(reportDesc),
    1,
    {
        1,    // numEndpoints
        0x03, /// class code - HID
        0x01, // subclass (boot interface)
        0x01, // protocol (keyboard)
        0x00, //
        0x01, //
    },
    {USB_EP_TYPE_INTERRUPT, 1},
    {USB_EP_TYPE_INTERRUPT, 1},
};

USBHIDKeyboard::USBHIDKeyboard() : USBHID()
{
}

int USBHIDKeyboard::stdRequest(UsbEndpointIn &ctrl, USBSetup &setup)
{
    if (setup.bRequest == GET_DESCRIPTOR)
    {
        if (setup.wValueH == 0x21)
        {
            InterfaceDescriptor tmp;
            fillInterfaceInfo(&tmp);
            return ctrl.write(&tmp, sizeof(tmp));
        }
        else if (setup.wValueH == 0x22)
        {
            return ctrl.write(hidKeyboardDescriptor, sizeof(hidKeyboardDescriptor));
        }
    }
    return DEVICE_NOT_SUPPORTED;
}

const InterfaceInfo *USBHIDKeyboard::getInterfaceInfo()
{
    return &ifaceInfo;
}

int USBHIDKeyboard::keyPress(uint8_t key)
{
    const uint8_t report[9] = {0x01,0x00,0x00,key,0x00,0x00,0x00,0x00,0x00};
    return in->write(report, sizeof(report));
}

int USBHIDKeyboard::keyUp()
{
    const uint8_t report[9] = {0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    return in->write(report, sizeof(report));
}

#endif

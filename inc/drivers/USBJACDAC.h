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

#ifndef DEVICE_USB_JACDAC_H
#define DEVICE_USB_JACDAC_H

#include "CodalUSB.h"
#include "JACDAC.h"

#define USB_JACDAC_BUFFER_SIZE      2048

#if CONFIG_ENABLED(DEVICE_USB)

namespace codal
{
    class USBJACDAC : public CodalUSBInterface, public JDService
    {
        uint8_t* inBuf;
        uint8_t* outBuf;

        uint16_t inBuffPtr;
        uint16_t outBuffPtr;

        public:
        USBJACDAC();

        virtual int classRequest(UsbEndpointIn &ctrl, USBSetup& setup) override;
        virtual int stdRequest(UsbEndpointIn &ctrl, USBSetup& setup) override;
        virtual int endpointRequest() override;
        virtual const InterfaceInfo *getInterfaceInfo() override;
        virtual int handlePacket(JDPacket*) override;

        virtual void idleCallback() override;
    };
}

#endif

#endif
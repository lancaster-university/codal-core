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

#ifndef CODAL_PKTSERIAL_PROTOCOL_H
#define CODAL_PKTSERIAL_PROTOCOL_H

#include "CodalConfig.h"
#include "CodalComponent.h"
#include "ErrorNo.h"
#include "Event.h"
#include "PktSerial.h"
#include "codal_target_hal.h"

#define PKT_DRIVER_ADDRESS_SIZE         2

#define PKT_PROTOCOL_EVT_SEND_CONTROL   1

#define PKT_DEVICE_CLASS_ARCADE         1
#define PKT_DEVICE_CLASS_JOYSTICK       2

#define CONTROL_PKT_FLAGS_BROADCAST     0x0001
#define CONTROL_PKT_FLAGS_PAIRED        0x0002
#define CONTROL_PKT_FLAGS_UNCERTAIN     0x0004

#define CONTROL_PKT_TYPE_HELLO          0x01

#define PKT_DEVICE_FLAG_DRIVING         0x0001

namespace codal
{
    struct ControlPacket
    {
        uint8_t packet_type;
        uint8_t address;
        uint16_t flags;
        uint32_t device_class;
        uint32_t serial_number;
    };

    struct PktDevice
    {
        uint8_t address;
        uint8_t timeout_counter;
        uint16_t flags;
        uint32_t serial_number;
    };

    class PktSerialDriver : public CodalComponent
    {
        PktSerial& serial;

        protected:
        uint32_t device_class;
        PktDevice devices[PKT_DRIVER_ADDRESS_SIZE];

        public:
        PktSerialDriver(PktSerial& pkt, uint32_t device_class) : serial(pkt)
        {
            memset((uint8_t*)&devices, 0, PKT_DRIVER_ADDRESS_SIZE * sizeof(PktDevice));
        }

        virtual int queueControlPackets()
        {
            for (int  i = 0 ; i < PKT_DRIVER_ADDRESS_SIZE; i++)
            {
                if (devices[i].serial_number > 0 && devices[i].flags & PKT_DEVICE_FLAG_DRIVING)
                {
                    ControlPacket cp;

                    cp.packet_type = CONTROL_PKT_TYPE_HELLO;
                    cp.address = (devices[i].address > 0) ? devices[i].address : target_random(255);
                    cp.flags = devices[i].flags;
                    cp.device_class = this->device_class;
                    cp.serial_number = devices[i].serial_number;

                    serial.send((uint8_t *)&cp, sizeof(ControlPacket), 0);
                }
            }

            return DEVICE_OK;
        }

        int deviceConnected(PktDevice device)
        {
            int i;

            for (i = 0; i < PKT_DRIVER_ADDRESS_SIZE; i++)
            {
                if (devices[i].serial_number == 0)
                {
                    devices[i] = device;
                    break;
                }
            }

            if (i == PKT_DRIVER_ADDRESS_SIZE)
                return DEVICE_NO_RESOURCES;

            return DEVICE_OK;
        }

        virtual void handlePacket(PktSerialPkt* p) = 0;
    };

    class PktSerialProtocol : public CodalComponent
    {
        PktSerial& bus;

        void onPacketReceived(Event);
        void sendControl(Event);

    public:

        virtual void periodicCallback();

        PktSerialProtocol(PktSerial& pkt, uint32_t serialNumber, uint16_t id = DEVICE_ID_PKTSERIAL_PROTOCOL);

        virtual int add(PktSerialDriver& device);

        virtual int remove(PktSerialDriver& device);
    };

} // namespace codal

#endif

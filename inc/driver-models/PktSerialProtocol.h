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


// the following defines should really be in separate head files, but circular includes suck.

// BEGIN    PKT SERIAL DRIVER FLAGS
#define PKT_DRIVER_EVT_CONNECTED    1

#define PKT_DEVICE_FLAGS_LOCAL          0x8000 // on the board
#define PKT_DEVICE_FLAGS_REMOTE         0x4000 // off the board
#define PKT_DEVICE_FLAGS_INITIALISED    0x2000 // off the board
#define PKT_DEVICE_CTRL_PACKET_QUEUED   0x1000 // a flag to indicate that a control packet has been queued
// END      PKT SERIAL DRIVER FLAGS


// BEGIN    LOGIC DRIVER FLAGS
#define PKT_LOGIC_DRIVER_MAX_FILTERS        20
#define PKT_LOGIC_DRIVER_TIMEOUT            254     // 1,016 ms
#define PKT_LOGIC_DRIVER_CTRLPACKET_TIME    112     // 448 ms

#define CONTROL_PKT_FLAGS_BROADCAST     0x0001
#define CONTROL_PKT_FLAGS_PAIRED        0x0002
#define CONTROL_PKT_FLAGS_UNCERTAIN     0x0004
#define CONTROL_PKT_TYPE_HELLO          0x01
// END      LOGIC DRIVER FLAGS


// BEGIN    PKT SERIAL PROTOCOL
#define PKT_PROTOCOL_EVT_SEND_CONTROL   1
#define PKT_PROTOCOL_DRIVER_SIZE        10

#define PKT_DRIVER_CLASS_CONTROL        0
#define PKT_DRIVER_CLASS_ARCADE         1
#define PKT_DRIVER_CLASS_JOYSTICK       2
// END      PKT SERIAL PROTOCOL

namespace codal
{
    class PktSerialProtocol;

    struct ControlPacket
    {
        uint8_t packet_type;
        uint8_t address;
        uint16_t flags;
        uint32_t driver_class;
        uint32_t serial_number;
    };

    struct PktDevice
    {
        uint8_t address;
        uint8_t rolling_counter;
        uint16_t flags; // upper 8 bits can be used by drivers, lower 8 bits are placed into the control packet
        uint32_t serial_number;

        PktDevice()
        {
            address = 0;
            rolling_counter = 0;
            flags = PKT_DEVICE_FLAGS_REMOTE;
            serial_number = 0;
        }

        PktDevice(uint8_t address, uint8_t rolling_counter, uint16_t flags, uint32_t serial_number)
        {
            this->address = address;
            this->rolling_counter = rolling_counter;
            this->flags = flags;
            this->serial_number = serial_number;
        }
    };

    class PktSerialDriver : public CodalComponent
    {
        friend class PktLogicDriver;
        friend class PktSerialProtocol;

        protected:
        PktSerialProtocol& proto;
        uint32_t driver_class;
        PktDevice device;

        public:

        PktSerialDriver(PktSerialProtocol& proto, PktDevice d, uint32_t driver_class, uint16_t id);

        virtual int queueControlPacket();
        virtual int deviceConnected(PktDevice device);
        virtual int deviceRemoved();

        virtual void handleControlPacket(ControlPacket* cp) = 0;
        virtual void handlePacket(PktSerialPkt* p) = 0;
    };

    class PktLogicDriver : public PktSerialDriver
    {
        uint8_t address_filters[PKT_LOGIC_DRIVER_MAX_FILTERS];

        public:
        virtual void periodicCallback();

        PktLogicDriver(PktSerialProtocol& proto, PktDevice d = PktDevice(), uint32_t driver_class = PKT_DRIVER_CLASS_CONTROL, uint16_t id = DEVICE_ID_PKT_LOGIC_DRIVER);

        virtual void handleControlPacket(ControlPacket* p);

        virtual void handlePacket(PktSerialPkt* p);

        virtual bool filterPacket(uint8_t address);
    };

    class PktSerialProtocol : public CodalComponent
    {
        friend class PktLogicDriver;

        void onPacketReceived(Event);

        static PktSerialDriver* drivers[PKT_PROTOCOL_DRIVER_SIZE];

        PktLogicDriver logic;

    public:
        PktSerial& bus;

        PktSerialProtocol(PktSerial& pkt, uint16_t id = DEVICE_ID_PKTSERIAL_PROTOCOL);

        virtual int add(PktSerialDriver& device);

        virtual int remove(PktSerialDriver& device);
    };

} // namespace codal

#endif

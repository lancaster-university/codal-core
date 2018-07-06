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
#define PKT_DRIVER_EVT_CONNECTED        1
#define PKT_DRIVER_EVT_DISCONNECTED     2

#define PKT_DEVICE_FLAGS_LOCAL          0x8000 // on the board
#define PKT_DEVICE_FLAGS_REMOTE         0x4000 // off the board
#define PKT_DEVICE_FLAGS_INITIALISED    0x2000 // off the board
#define PKT_DEVICE_FLAGS_INITIALISING   0x1000 // a flag to indicate that a control packet has been queued
#define PKT_DEVICE_FLAGS_CP_SEEN        0x0800 // indicates whether a control packet has been seen recently.
// END      PKT SERIAL DRIVER FLAGS


// BEGIN    LOGIC DRIVER FLAGS
#define PKT_LOGIC_DRIVER_MAX_FILTERS        20
#define PKT_LOGIC_DRIVER_TIMEOUT            254     // 1,016 ms
#define PKT_LOGIC_ADDRESS_ALLOC_TIME        254     // 1,016 ms
#define PKT_LOGIC_DRIVER_CTRLPACKET_TIME    112     // 448 ms

#define CONTROL_PKT_FLAGS_BROADCAST     0x0001
#define CONTROL_PKT_FLAGS_PAIRED        0x0002
#define CONTROL_PKT_FLAGS_UNCERTAIN     0x0004
#define CONTROL_PKT_FLAGS_CONFLICT      0x0008
#define CONTROL_PKT_TYPE_HELLO          0x01
// END      LOGIC DRIVER FLAGS


// BEGIN    PKT SERIAL PROTOCOL
#define PKT_PROTOCOL_EVT_SEND_CONTROL   1
#define PKT_PROTOCOL_DRIVER_SIZE        10

#define PKT_DRIVER_CLASS_CONTROL        0
#define PKT_DRIVER_CLASS_ARCADE         1
#define PKT_DRIVER_CLASS_JOYSTICK       2
#define PKT_DRIVER_CLASS_MESSAGE_BUS    3
// END      PKT SERIAL PROTOCOL

namespace codal
{
    class PktSerialProtocol;

    /**
     * This struct represents a ControlPacket used by the logic driver
     **/
    struct ControlPacket
    {
        uint8_t packet_type;    // indicates the type of the packet, normally just HELLO
        uint8_t address;        // the address assigned by the logic driver
        uint16_t flags;         // various flags
        uint32_t driver_class;  // the class of the driver
        uint32_t serial_number; // the "unique" serial number of the device.
    };

    /**
     * This struct represents a PktDevice used by a Device driver
     **/
    struct PktDevice
    {
        uint8_t address; // the address assigned by the logic driver.
        uint8_t rolling_counter; // used to trigger various time related events
        uint16_t flags; // upper 8 bits can be used by drivers, lower 8 bits are placed into the control packet
        uint32_t serial_number; // the serial number used to "uniquely" identify a device

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

    /**
     * This class presents a common abstraction for all PktSerialDrivers. It also contains some default member functions to perform common operations.
     * This should be subclassed by any driver implementation
     **/
    class PktSerialDriver : public CodalComponent
    {
        friend class PktLogicDriver;
        friend class PktSerialProtocol;

        protected:
        PktSerialProtocol& proto;
        uint32_t driver_class;
        PktDevice device;

        public:

        /**
         * Constructor
         *
         * @param proto a reference to PktSerialProtocol instance
         *
         * @param d a struct containing a device representation
         *
         * @param driver_class a number that represents this unique driver class
         *
         * @param id the message bus id for this driver
         *
         * */
        PktSerialDriver(PktSerialProtocol& proto, PktDevice d, uint32_t driver_class, uint16_t id);

        /**
         * Queues a control packet on the serial bus, called by the logic driver
         **/
        virtual int queueControlPacket();

        /**
         * Returns the current connected state of this Serial driver instance.
         *
         * @return true for connected, false for disconnected
         **/
        virtual bool isConnected();

        /**
         * Called by the logic driver when a new device is connected to the serial bus
         *
         * @param device an instance of PktDevice representing the device that has been connected
         *
         * @return DEVICE_OK for success
         **/
        virtual int deviceConnected(PktDevice device);

        /**
         * Called by the logic driver when an existing device is disconnected from the serial bus
         *
         * @return DEVICE_OK for success
         **/
        virtual int deviceRemoved();

        /**
         * Called by the logic driver when a control packet is addressed to this driver
         *
         * @param cp the control packet from the serial bus.
         **/
        virtual void handleControlPacket(ControlPacket* cp) = 0;

        /**
         * Called by the logic driver when a data packet is addressed to this driver
         *
         * @param cp the control packet from the serial bus.
         **/
        virtual void handlePacket(PktSerialPkt* p) = 0;
    };

    class PktLogicDriver : public PktSerialDriver
    {
        uint8_t address_filters[PKT_LOGIC_DRIVER_MAX_FILTERS];

        public:

        /**
         *  used to detect when a remote device disconnects, and triggers various events for local drivers
         * */
        virtual void periodicCallback();

        /**
         * Constructor
         *
         * @param proto a reference to PktSerialProtocol instance
         *
         * @param d a struct containing a device representation
         *
         * @param driver_class a number that represents this unique driver class
         *
         * @param id the message bus id for this driver
         *
         * */
        PktLogicDriver(PktSerialProtocol& proto, PktDevice d = PktDevice(), uint32_t driver_class = PKT_DRIVER_CLASS_CONTROL, uint16_t id = DEVICE_ID_PKT_LOGIC_DRIVER);

        /**
         * Called by the logic driver when a control packet is addressed to this driver
         *
         * @param cp the control packet from the serial bus.
         **/
        virtual void handleControlPacket(ControlPacket* p);

        /**
         * Called by the logic driver when a data packet is addressed to this driver
         *
         * @param cp the control packet from the serial bus.
         **/
        virtual void handlePacket(PktSerialPkt* p);

        /**
         * This function provides the ability to ignore specific packets. For instance, we are not interested in packets that are paired to other devices
         * hence we shouldn't incur the processing cost.
         *
         * @param address the address to check in the filter.
         *
         * @return true if the packet should be filtered, false if it should pass through.
         **/
        virtual bool filterPacket(uint8_t address);

        /**
         * Begin periodic callbacks
         * */
        void start();

        /**
         * End periodic callbacks.
         * */
        void stop();
    };

    class PktSerialProtocol : public CodalComponent
    {
        friend class PktLogicDriver;

        void onPacketReceived(Event);

        static PktSerialDriver* drivers[PKT_PROTOCOL_DRIVER_SIZE];

        PktLogicDriver logic;

    public:
        PktSerial& bus;

        /**
         * Constructor
         *
         * @param pkt A reference to PktSerial for communicators
         *
         * @param id for the message bus, defaults to  DEVICE_ID_PKTSERIAL_PROTOCOL
         **/
        PktSerialProtocol(PktSerial& pkt, uint16_t id = DEVICE_ID_PKTSERIAL_PROTOCOL);

        /**
         * Adds a driver to the drivers array. The logic driver iterates over this array.
         *
         * @param device a reference to the driver to add.
         *
         * @note please call stop() before adding a driver, then resume by calling start
         **/
        virtual int add(PktSerialDriver& device);

        /**
         * removes a driver from the drivers array. The logic driver iterates over this array.
         *
         * @param device a reference to the driver to remove.
         *
         * @note please call stop() before removing a driver, then resume by calling start()
         **/
        virtual int remove(PktSerialDriver& device);

        /**
         * Begin logic driver periodic callbacks
         * */
        void start();

        /**
         * End logic driver periodic callbacks.
         * */
        void stop();
    };

} // namespace codal

#endif

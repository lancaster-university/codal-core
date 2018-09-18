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

#ifndef CODAL_JACDAC_PROTOCOL_H
#define CODAL_JACDAC_PROTOCOL_H

#include "CodalConfig.h"
#include "CodalComponent.h"
#include "ErrorNo.h"
#include "Event.h"
#include "JACDAC.h"
#include "codal_target_hal.h"


// the following defines should really be in separate head files, but circular includes suck.

// BEGIN    JD SERIAL DRIVER FLAGS
#define JD_DRIVER_EVT_CONNECTED        1
#define JD_DRIVER_EVT_DISCONNECTED     2

#define JD_DEVICE_FLAGS_LOCAL          0x8000 // on the board
#define JD_DEVICE_FLAGS_REMOTE         0x4000 // off the board

// following flags combined with the above to yield different behaviours
#define JD_DEVICE_FLAGS_BROADCAST      0x2000 // receive all class packets regardless of the address
#define JD_DEVICE_FLAGS_PAIRABLE       0x1000 // this flag indicates that a driver is paired with another
// end combo flags

#define JD_DEVICE_FLAGS_INITIALISED    0x0800 // device driver is running
#define JD_DEVICE_FLAGS_INITIALISING   0x0400 // a flag to indicate that a control packet has been queued
#define JD_DEVICE_FLAGS_CP_SEEN        0x0200 // indicates whether a control packet has been seen recently.
#define JD_DEVICE_FLAGS_BROADCAST_MAP  0x0100 // This driver is held for mapping from bus address to driver class
// END      JD SERIAL DRIVER FLAGS


// BEGIN    LOGIC DRIVER FLAGS
#define JD_LOGIC_DRIVER_MAX_FILTERS        20
#define JD_LOGIC_DRIVER_TIMEOUT            254     // 1,016 ms
#define JD_LOGIC_ADDRESS_ALLOC_TIME        254     // 1,016 ms
#define JD_LOGIC_DRIVER_CTRLPACKET_TIME    112     // 448 ms

#define CONTROL_JD_FLAGS_PAIRED                 0x0001
#define CONTROL_JD_FLAGS_PAIRABLE               0x0002
#define CONTROL_JD_FLAGS_NACK                   0x0004
#define CONTROL_JD_FLAGS_UNCERTAIN              0x0008
#define CONTROL_JD_FLAGS_CONFLICT               0x0010

#define CONTROL_JD_TYPE_HELLO                   0x01
#define CONTROL_JD_TYPE_PAIRING_REQUEST         0x02
// END      LOGIC DRIVER FLAGS


// BEGIN    JD SERIAL PROTOCOL
#define JD_PROTOCOL_EVT_SEND_CONTROL   1
#define JD_PROTOCOL_DRIVER_SIZE        10

#define JD_DRIVER_CLASS_CONTROL        0
#define JD_DRIVER_CLASS_ARCADE         1
#define JD_DRIVER_CLASS_JOYSTICK       2
#define JD_DRIVER_CLASS_MESSAGE_BUS    3
#define JD_DRIVER_CLASS_RADIO          4
#define JD_DRIVER_CLASS_BRIDGE         5
#define JD_DRIVER_CLASS_BUTTON         6
// END      JD SERIAL PROTOCOL

#define CONTROL_PACKET_PAYLOAD_SIZE     (JD_SERIAL_DATA_SIZE - 12)

namespace codal
{
    class JDProtocol;

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
        uint8_t data[CONTROL_PACKET_PAYLOAD_SIZE];
    };

    enum DriverType
    {
        VirtualDriver = JD_DEVICE_FLAGS_REMOTE, // the driver is seeking the use of another device's resource
        HostDriver = JD_DEVICE_FLAGS_LOCAL, // the driver is hosting a resource for others to use.
        HostDriverPairable = JD_DEVICE_FLAGS_PAIRABLE | JD_DEVICE_FLAGS_LOCAL, // the driver is allowed to pair with another driver of the same class
        BroadcastDriver = JD_DEVICE_FLAGS_LOCAL | JD_DEVICE_FLAGS_BROADCAST, // the driver is unumerated with its own address, and receives all packets of the same class (including control packets)
        SnifferDriver = JD_DEVICE_FLAGS_REMOTE | JD_DEVICE_FLAGS_BROADCAST, // the driver is not unumerated, and receives all packets of the same class (including control packets)
    };

    /**
     * This struct represents a JDDevice used by a Device driver
     **/
    struct JDDevice
    {
        uint8_t address; // the address assigned by the logic driver.
        uint8_t rolling_counter; // used to trigger various time related events
        uint16_t flags; // upper 8 bits can be used by drivers, lower 8 bits are placed into the control packet
        uint32_t serial_number; // the serial number used to "uniquely" identify a device
        uint32_t driver_class;

        JDDevice(uint32_t driver_class)
        {
            address = 0;
            rolling_counter = 0;
            flags = JD_DEVICE_FLAGS_LOCAL;
            serial_number = (target_get_serial() & 0xffffff00) | driver_class;
            driver_class = driver_class;
        }

        JDDevice(DriverType t, uint32_t driver_class)
        {
            this->address = 0;
            this->rolling_counter = 0;
            this->flags |= t;
            this->serial_number = (target_get_serial() & 0xffffff00) | driver_class;
            this->driver_class = driver_class;
        }

        JDDevice(uint8_t address, uint16_t flags, uint32_t serial_number, uint32_t driver_class)
        {
            this->address = address;
            this->rolling_counter = 0;
            this->flags |= flags;
            this->serial_number = serial_number;
            this->driver_class = driver_class;
        }
    };

    /**
     * This class presents a common abstraction for all JDDrivers. It also contains some default member functions to perform common operations.
     * This should be subclassed by any driver implementation
     **/
    class JDDriver : public CodalComponent
    {
        friend class JDLogicDriver;
        friend class JDProtocol;

        static uint32_t dynamicId;

        protected:
        JDDriver* pairedInstance;

        JDDevice device;

        int handleLogicPacket(JDPkt* p);

        public:

        /**
         * Constructor
         *
         * @param proto a reference to JDProtocol instance
         *
         * @param d a struct containing a device representation
         *
         * @param driver_class a number that represents this unique driver class
         *
         * @param id the message bus id for this driver
         *
         * */
        JDDriver(JDDevice d, uint16_t id);

        /**
         * Queues a control packet on the serial bus, called by the logic driver
         **/
        virtual int fillControlPacket(JDPkt* p);

        /**
         * Returns the current connected state of this Serial driver instance.
         *
         * @return true for connected, false for disconnected
         **/
        virtual bool isConnected();

        /**
         * Called by the logic driver when a new device is connected to the serial bus
         *
         * @param device an instance of JDDevice representing the device that has been connected
         *
         * @return DEVICE_OK for success
         **/
        virtual int deviceConnected(JDDevice device);

        /**
         * Called by the logic driver when an existing device is disconnected from the serial bus
         *
         * @return DEVICE_OK for success
         **/
        virtual int deviceRemoved();

        int sendPairingRequest(JDPkt* p);

        virtual bool isPaired();

        virtual bool isPairable();

        void partnerDisconnected(Event);

        /**
         * Called by the logic driver when a control packet is addressed to this driver
         *
         * @param cp the control packet from the serial bus.
         **/
        virtual int handleControlPacket(JDPkt* p);

        int handlePairingRequest(JDPkt* p);

        /**
         * Called by the logic driver when a data packet is addressed to this driver
         *
         * @param cp the control packet from the serial bus.
         **/
        virtual int handlePacket(JDPkt* p);

        ~JDDriver();
    };

    class JDLogicDriver : public JDDriver
    {
        uint8_t address_filters[JD_LOGIC_DRIVER_MAX_FILTERS];

        public:

        /**
         *  used to detect when a remote device disconnects, and triggers various events for local drivers
         * */
        virtual void periodicCallback();

        /**
         * Constructor
         *
         * @param proto a reference to JDProtocol instance
         *
         * @param d a struct containing a device representation
         *
         * @param driver_class a number that represents this unique driver class
         *
         * @param id the message bus id for this driver
         *
         * */
        JDLogicDriver(JDDevice d = JDDevice(0, JD_DEVICE_FLAGS_LOCAL | JD_DEVICE_FLAGS_INITIALISED, 0, 0), uint16_t id = DEVICE_ID_JD_LOGIC_DRIVER);

        /**
         * Called by the logic driver when a control packet is addressed to this driver
         *
         * @param cp the control packet from the serial bus.
         **/
        virtual int handleControlPacket(JDPkt* p);

        /**
         * Called by the logic driver when a data packet is addressed to this driver
         *
         * @param cp the control packet from the serial bus.
         **/
        virtual int handlePacket(JDPkt* p);

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

    class JDProtocol : public CodalComponent
    {
        friend class JDLogicDriver;

        void onPacketReceived(Event);

        static JDDriver* drivers[JD_PROTOCOL_DRIVER_SIZE];

        JDLogicDriver logic;
        JDDriver* bridge;

    public:
        JACDAC& bus;

        static JDProtocol* instance;

        /**
         * Constructor
         *
         * @param JD A reference to JACDAC for communicators
         *
         * @param id for the message bus, defaults to  DEVICE_ID_JACDAC_PROTOCOL
         **/
        JDProtocol(JACDAC& JD, uint16_t id = DEVICE_ID_JACDAC_PROTOCOL);

        int setBridge(JDDriver& bridge);

        /**
         * Adds a driver to the drivers array. The logic driver iterates over this array.
         *
         * @param device a reference to the driver to add.
         *
         * @note please call stop() before adding a driver, then resume by calling start
         **/
        virtual int add(JDDriver& device);

        /**
         * removes a driver from the drivers array. The logic driver iterates over this array.
         *
         * @param device a reference to the driver to remove.
         *
         * @note please call stop() before removing a driver, then resume by calling start()
         **/
        virtual int remove(JDDriver& device);

        /**
         * Begin logic driver periodic callbacks
         * */
        void start();

        /**
         * End logic driver periodic callbacks.
         * */
        void stop();

        static int send(JDPkt* JD);
        static int send(uint8_t* buf, int len, uint8_t address);
    };

} // namespace codal

#endif

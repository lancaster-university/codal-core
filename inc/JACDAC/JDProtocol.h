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
#include "JackRouter.h"
#include "ManagedString.h"
#include "codal_target_hal.h"


// the following defines should really be in separate head files, but circular includes suck.

// BEGIN    JD SERIAL DRIVER FLAGS
#define JD_DRIVER_EVT_CONNECTED         65520
#define JD_DRIVER_EVT_DISCONNECTED      65521
#define JD_DRIVER_EVT_PAIRED            65522
#define JD_DRIVER_EVT_UNPAIRED          65523
#define JD_DRIVER_EVT_PAIR_REJECTED     65524
#define JD_DRIVER_EVT_PAIRING_RESPONSE  65525
#define JD_DRIVER_EVT_ERROR             65526

#define JD_DEVICE_FLAGS_LOCAL           0x8000 // on the board
#define JD_DEVICE_FLAGS_REMOTE          0x4000 // off the board

// following flags combined with the above to yield different behaviours
#define JD_DEVICE_FLAGS_BROADCAST       0x2000 // receive all class packets regardless of the address
#define JD_DEVICE_FLAGS_PAIR            0x1000 // this flag indicates that the driver should pair with another

#define JD_DEVICE_DRIVER_MODE_MSK       0xF000 // the top byte represents the current driver mode.
// end combo flags

#define JD_DEVICE_FLAGS_PAIRABLE        0x0800 // this flag indicates that a driver is paired with another
#define JD_DEVICE_FLAGS_PAIRED          0x0400 // this flag indicates that a driver is paired with another
#define JD_DEVICE_FLAGS_PAIRING         0x0200 // this flag indicates that a driver is paired with another

#define JD_DEVICE_FLAGS_INITIALISED     0x0100 // device driver is running
#define JD_DEVICE_FLAGS_INITIALISING    0x0080 // a flag to indicate that a control packet has been queued
#define JD_DEVICE_FLAGS_CP_SEEN         0x0040 // indicates whether a control packet has been seen recently.

#define JD_DEVICE_COMM_RATE_MSK         0x0030 // these bits indicate the current comm rate of the driver
#define JD_DEVICE_COMM_RATE_POS         16     // the position of the comm rate msk in the flags field.

#define JD_DEVICE_ERROR_MSK             0x000F // the lower 4 bits are reserved for well known errors, these are
                                               // automatically placed into control packets by the logic driver.
// END      JD SERIAL DRIVER FLAGS


// BEGIN    LOGIC DRIVER FLAGS
#define JD_LOGIC_DRIVER_MAX_FILTERS                     20
#define JD_LOGIC_DRIVER_EVT_CHANGED                     2
#define JD_LOGIC_DRIVER_EVT_TIMER_CALLBACK              3

#define JD_DRIVER_INFO_FLAGS_RESERVED                   0x80
#define JD_DRIVER_INFO_FLAGS_PAIRING_MODE               0x40 // in pairing mode, control packets aren't forwarded to drivers
#define JD_DRIVER_INFO_FLAGS_PAIRABLE                   0x20 // advertises that a driver can be optionally paired with another
#define JD_DRIVER_INFO_FLAGS_PAIRED                     0x10 // advertises that a driver is already paired with another.

#define JD_DRIVER_INFO_FLAGS_CONFLICT                   0x08
#define JD_DRIVER_INFO_FLAGS_UNCERTAIN                  0x04
#define JD_DRIVER_INFO_FLAGS_NACK                       0x02
#define JD_DRIVER_INFO_FLAGS_ACK                        0x01

#define JD_DRIVER_INFO_TYPE_HELLO                       0x1
#define JD_DRIVER_INFO_TYPE_PAIRING_REQUEST             0x2
#define JD_DRIVER_INFO_TYPE_ERROR                       0x3
#define JD_DRIVER_INFO_TYPE_PANIC                       0xF

#define JD_DRIVER_INFO_MAX_PAYLOAD_SIZE                 16
// END      LOGIC DRIVER FLAGS


// BEGIN    JD SERIAL PROTOCOL
#define JD_PROTOCOL_EVT_SEND_CONTROL                1
#define JD_PROTOCOL_DRIVER_ARRAY_SIZE               20

#include "JDClasses.h"

// END      JD SERIAL PROTOCOL
#define JD_CONTROL_PACKET_ERROR_NAME_LENGTH         6

#define JD_DRIVER_INFO_HEADER_SIZE                  8
#define JD_CONTROL_PACKET_HEADER_SIZE               4

#define JD_MAX_PACKET_SIZE                          255

namespace codal
{
    class JDProtocol;

    /**
     * This struct represents a JDControlPacket used by the logic driver
     * A control packet provides full information about a driver, it's most important use is to translates the address used in
     * standard packets to the full driver information. Standard packet address == control packet address.
     *
     * Currently there are two types of packet:
     * CONTROL_JD_TYPE_HELLO - Which broadcasts the availablity of a driver
     * CONTROL_JD_TYPE_PAIRING_REQUEST - Used when drivers are pairing to one another.
     **/
    struct JDControlPacket
    {
        uint32_t serial_number; // the "unique" serial number of the device.
        uint8_t data[];
    } __attribute((__packed__));

    struct JDDriverInfo
    {
        uint8_t size;
        uint8_t address;        // the address assigned by the logic driver
        uint16_t flags: 8, error_code: 4, type: 4; // various flags, upper eight bits are reserved for control usage, error code, then driver info type
        uint32_t driver_class;  // the class of the driver
        uint8_t data[]; // optional additional data, maximum of 16 bytes
    } __attribute((__packed__));

    // This enumeration specifies that supported configurations that drivers should utilise.
    // Many combinations of flags are supported, but only the ones listed here have been fully implemented.
    enum DriverType
    {
        VirtualDriver = JD_DEVICE_FLAGS_REMOTE, // the driver is seeking the use of another device's resource
        PairedDriver = JD_DEVICE_FLAGS_BROADCAST | JD_DEVICE_FLAGS_PAIR,
        HostDriver = JD_DEVICE_FLAGS_LOCAL, // the driver is hosting a resource for others to use.
        PairableHostDriver = JD_DEVICE_FLAGS_PAIRABLE | JD_DEVICE_FLAGS_LOCAL, // the driver is allowed to pair with another driver of the same class
        BroadcastDriver = JD_DEVICE_FLAGS_LOCAL | JD_DEVICE_FLAGS_BROADCAST, // the driver is enumerated with its own address, and receives all packets of the same class (including control packets)
        SnifferDriver = JD_DEVICE_FLAGS_REMOTE | JD_DEVICE_FLAGS_BROADCAST, // the driver is not enumerated, and receives all packets of the same class (including control packets)
    };

    enum DriverErrorCode
    {
        // No error occurred.
        DRIVER_OK = 0,

        // Device calibration information
        DRIVER_CALIBRATION_IN_PROGRESS,
        DRIVER_CALIBRATION_REQUIRED,

        // The driver has run out of some essential resource (e.g. allocated memory)
        DRIVER_NO_RESOURCES,

        // The driver operation could not be performed as some essential resource is busy (e.g. the display)
        DRIVER_BUSY,

        // I2C / SPI Communication error occured
        DRIVER_COMMS_ERROR,

        // An invalid state was detected (i.e. not initialised)
        DRIVER_INVALID_STATE,

        // an external peripheral has a malfunction e.g. external circuitry is drawing too much power.
        DRIVER_PERIPHERAL_MALFUNCTION
    };

    /**
     * This struct represents a JDDevice used by a JDDriver.
     *
     * It is perhaps named incorrectly, but JDDevice represents the core information about the driver which is placed into control packets.
     * A rolling counter is used to trigger control packets and other core driver events.
     **/
    struct JDDevice
    {
        uint8_t address; // the address assigned by the logic driver.
        uint8_t rolling_counter; // used to trigger various time related events
        uint16_t flags; // various flags indicating the state of the driver
        uint32_t serial_number; // the serial number used to "uniquely" identify a device
        uint32_t driver_class; // the class of the driver, created or selected from the list in JDClasses.h

        /**
         * Constructor, creates a local driver using just the driver class.
         *
         * Should be used if only a local driver is required.
         *
         * @param driver_class the class of the driver listed in JDClasses.h
         **/
        JDDevice(uint32_t driver_class)
        {
            address = 0;
            rolling_counter = 0;
            flags = JD_DEVICE_FLAGS_LOCAL;
            serial_number = target_get_serial();
            driver_class = driver_class;
        }

        /**
         * Constructor, creates a driver given a DriverType (enumeration above) and the driver class.
         *
         * Should be used if you need to use any of the other types from the enumeration (most of the time, this will be used).
         *
         * @param t the driver type to use
         *
         * @param driver_class the class of the driver listed in JDClasses.h
         *
         * @note the VirtualDriver will always have a serial_number of 0 by default, as the serial number is used as a filter.
         *       if a filter is required, then the full constructor should be used (below).
         **/
        JDDevice(DriverType t, uint32_t driver_class)
        {
            this->address = 0;
            this->rolling_counter = 0;
            this->flags |= t;

            if (t & JD_DEVICE_FLAGS_REMOTE)
                this->serial_number = 0;
            else
                this->serial_number = target_get_serial();

            this->driver_class = driver_class;
        }

        /**
         * Constructor, allows (almost) full specification of a JDDevice.
         *
         * Should be used if you need to specify all of the fields, i.e. if you're adding complex logic that requires already
         * initialised drivers.
         *
         * @param a the address of the driver
         *
         * @param flags the low-level flags that are normally set by using the DriverType enum.
         *
         * @param serial_number the serial number of the driver
         *
         * @param driver_class the class of the driver listed in JDClasses.h
         *
         * @note you are responsible for any weirdness you achieve using this constructor.
         **/
        JDDevice(uint8_t address, uint16_t flags, uint32_t serial_number, uint32_t driver_class)
        {
            this->address = address;
            this->rolling_counter = 0;
            this->flags = flags;
            this->serial_number = serial_number;
            this->driver_class = driver_class;
        }

        /**
         * Sets the mode to the given DriverType
         *
         * @param m the new mode the driver should move to.
         *
         * @param initialised whether the driver is initialised or not (defaults to false).
         **/
        void setMode(DriverType m, bool initialised = false)
        {
            this->flags &= ~JD_DEVICE_DRIVER_MODE_MSK;
            this->flags |= m;

            if (initialised)
                this->flags |= JD_DEVICE_FLAGS_INITIALISED;
            else
                this->flags &= ~JD_DEVICE_FLAGS_INITIALISED;
        }

        /**
         * Sets the error portion of flags to the given error code
         *
         * @param e the error code to place into control packets
         **/
        void setError(DriverErrorCode e)
        {
            uint32_t flags = this->flags & ~(JD_DEVICE_ERROR_MSK);
            this->flags = flags | (uint8_t) e;
        }

        /**
         * Retrieves the current error code from the error portion of flags.
         *
         * @return a JDDeviceErrorCode
         **/
        DriverErrorCode getError()
        {
            return (DriverErrorCode)(this->flags & JD_DEVICE_ERROR_MSK);
        }

        /**
         * Used to determine what mode the driver is currently in.
         *
         * This will check to see if the flags field resembles the VirtualDriver mode specified in the DriverType enumeration.
         *
         * @returns true if in VirtualDriver mode.
         **/
        bool isVirtualDriver()
        {
            return (this->flags & JD_DEVICE_FLAGS_REMOTE) && !(this->flags & JD_DEVICE_FLAGS_BROADCAST);
        }

        /**
         * Used to determine what mode the driver is currently in.
         *
         * This will check to see if the flags field resembles the PairedDriver mode specified in the DriverType enumeration.
         *
         * @returns true if in PairedDriver mode.
         **/
        bool isPairedDriver()
        {
            return this->flags & JD_DEVICE_FLAGS_BROADCAST && this->flags & JD_DEVICE_FLAGS_PAIR;
        }

        /**
         * Used to determine what mode the driver is currently in.
         *
         * This will check to see if the flags field resembles the HostDriver mode specified in the DriverType enumeration.
         *
         * @returns true if in SnifferDriver mode.
         **/
        bool isHostDriver()
        {
            return this->flags & JD_DEVICE_FLAGS_LOCAL && !(this->flags & JD_DEVICE_FLAGS_BROADCAST);
        }

        /**
         * Used to determine what mode the driver is currently in.
         *
         * This will check to see if the flags field resembles the BroadcastDriver mode specified in the DriverType enumeration.
         *
         * @returns true if in BroadcastDriver mode.
         **/
        bool isBroadcastDriver()
        {
            return this->flags & JD_DEVICE_FLAGS_LOCAL && this->flags & JD_DEVICE_FLAGS_BROADCAST;
        }

        /**
         * Used to determine what mode the driver is currently in.
         *
         * This will check to see if the flags field resembles the SnifferDriver mode specified in the DriverType enumeration.
         *
         * @returns true if in SnifferDriver mode.
         **/
        bool isSnifferDriver()
        {
            return this->flags & JD_DEVICE_FLAGS_REMOTE && this->flags & JD_DEVICE_FLAGS_BROADCAST;
        }

        /**
         * Indicates if the driver is currently paired to another.
         *
         * @returns true if paired
         **/
        bool isPaired()
        {
            return this->flags & JD_DEVICE_FLAGS_PAIRED;
        }

        /**
         * Indicates if the driver can be currently paired to another.
         *
         * @returns true if pairable
         **/
        bool isPairable()
        {
            return this->flags & JD_DEVICE_FLAGS_PAIRABLE;
        }

        /**
         * Indicates if the driver is currently in the process of pairing to another.
         *
         * @returns true if pairing
         **/
        bool isPairing()
        {
            return this->flags & JD_DEVICE_FLAGS_PAIRING;
        }
    };

    class JDPairedDriver;
    class JDProtocol;

    /**
     * This class presents a common abstraction for all JDDrivers. It also contains some default member functions to perform common operations.
     * This should be subclassed by any driver implementation
     **/
    class JDDriver : public CodalComponent
    {
        friend class JDLogicDriver;
        friend class JDProtocol;
        friend class JDBroadcastMap;
        // the above need direct access to our member variables and more.

        uint8_t rolling_counter;

        /**
         * After calling sendPairingPacket, this member function is called when the device is enumerated.
         *
         * It then creates a pairing control packet, and sends it to the remote instance.
         **/
        void pair();

        protected:

        // Due to the dynamic nature of JACDAC when a new driver is created, this variable is incremented.
        // JACDAC id's are allocated from 3000 - 4000
        static uint32_t dynamicId;

        // When we pair to another driver, this points to the stub of our partner.
        JDPairedDriver* pairedInstance;

        // A struct the represents the state of the driver.
        JDDevice device;

        /**
         * This method internally redirects specific packets from the control driver.
         *
         * i.e. it switches the type of the logic packet, and redirects it to handleControlPacket or handlePairingPacket accordingly.
         **/
        int handleLogicPacket(JDControlPacket* cp);

        /**
         * Called by the logic driver when a new device is connected to the serial bus
         *
         * @param device an instance of JDDevice representing the device that has been connected
         *
         * @return DEVICE_OK for success
         **/
        virtual int deviceConnected(JDDevice device);

        /**
         * Called by the logic driver when this driver has been disconnected from the serial bus.
         *
         * This is only called if a driver is in VirtualMode and the virtualised device disappears from the bus.
         *
         * @return DEVICE_OK for success
         **/
        virtual int deviceRemoved();

        /**
         * This should be called when a driver wishes to pair with another. A driver should first detect a driver in pairing mode
         * by observing packets in Broadcast mode. PairedDriver from the DriverType enumeration first starts in broadcast mode only,
         * observes packets looking for a device to pair with. When a pairable device appears, the driver enumerates, and sends a
         * pairing packet by calling this member function.
         *
         * @param d the device to pair too.
         *
         * @returns DEVICE_OK on success.
         **/
        virtual int sendPairingPacket(JDDevice d);

        /**
         * This is called when a paired driver is removed from the bus. It unpairs this driver instance, and fires an event
         * using the drivers id, and the event code JD_DRIVER_EVT_UNPAIRED.
         **/
        void partnerDisconnected(Event);

        public:

        /**
         * Constructor
         *
         * @param d a struct containing a device representation, see JDDevice.
         * */
        JDDriver(JDDevice d);

        /**
         * Invoked by the logic driver when it is queuing a control packet.
         *
         * This allows the addition of driver specific control packet information and the setting of any additional flags.
         *
         * @param p A pointer to the packet, where the data field contains a pre-filled control packet.
         *
         * @return DEVICE_OK on success
         **/
        virtual int populateDriverInfo(JDDriverInfo* info, uint8_t bytesRemaining);

        /**
         * Invoked by the logic driver when a control packet with the address of the driver is received.
         *
         * Control packets are routed by address, or by class in broadcast mode. Drivers
         * can override this function to handle additional payloads in control packet.s
         *
         * @param p the packet from the serial bus. Drivers should cast p->data to a JDControlPacket.
         *
         * @return DEVICE_OK to signal that the packet has been handled, or DEVICE_CANCELLED to indicate the logic driver
         *         should continue to search for a driver.
         **/
        virtual int handleControlPacket(JDControlPacket* info);

        /**
         * Invoked by the logic driver when a control packet with its type set to error is received.
         *
         *
         * @param p the packet from the serial bus. Drivers should cast p->data to a JDControlPacket,
         *          then JDControlPacket->data to ControlPacketError to obtain the error code.
         *
         * @return DEVICE_OK to signal that the packet has been handled, or DEVICE_CANCELLED to indicate the logic driver
         *         should continue to search for a driver.
         **/
        virtual int handleErrorPacket(JDControlPacket* info);

        /**
         * Invoked by the logic driver when a pairing packet with the address of the driver is received.
         *
         * Pairing packets are Control packets with the type set to CONTROL_JD_TYPE_PAIRING_REQUEST. They are routed by
         * address, or by class in broadcast mode. Drivers can override this function to handle additional payloads in
         * control packet.
         *
         * Pairing packets contain the source information of the device that sent the pairing request in cp->data;
         *
         * @param p the packet from the serial bus. Drivers should cast p->data to a JDControlPacket.
         *
         * @return DEVICE_OK to signal that the packet has been handled, or DEVICE_CANCELLED to indicate the logic driver
         *         should continue to search for a driver.
         **/
        virtual int handlePairingPacket(JDControlPacket* p);

        /**
         * Invoked by the Protocol driver when a standard packet with the address of the driver is received.
         *
         * @param p the packet from the serial bus. Drivers should cast p->data to their agreed upon structure..
         *
         * @return DEVICE_OK to signal that the packet has been handled, or DEVICE_CANCELLED to indicate the logic driver
         *         should continue to search for a driver.
         **/
        virtual int handlePacket(JDPacket* p);

        /**
         * Returns the current connected state of this driver instance.
         *
         * @return true for connected, false for disconnected
         **/
        virtual bool isConnected();

        /**
         * Returns the current pairing state of this driver instance.
         *
         * @return true for paired, false for unpaired
         **/
        virtual bool isPaired();

        /**
         * Returns whether the driver is advertising a pairable state
         *
         * @return true for paired, false for unpaired
         **/
        virtual bool isPairable();

        /**
         * Retrieves the address of the driver.
         *
         * @return the address.
         **/
        uint8_t getAddress();

        /**
         * Retrieves the class of the driver.
         *
         * @return the class.
         **/
        uint32_t getClass();

        /**
         * Retrieves the state of the driver.
         *
         * @return the internal driver state.
         **/
        JDDevice getState();

        /**
         * Retrieves the serial number in use by this driver.
         *
         * @return the serial number
         **/
        uint32_t getSerialNumber();

        /**
         * Destructor, removes this driver from the drivers array and deletes the pairedInstance member variable if allocated.
         **/
        ~JDDriver();
    };

    /**
     * This class is a stub of a remote driver that a local driver is paired with.
     *
     * It simply forwards all standard packets to the paired local driver for processing.
     **/
    class JDPairedDriver : public JDDriver
    {
        JDDriver& other;

        public:

        JDPairedDriver(JDDevice d, JDDriver& other) : JDDriver(d), other(other)
        {
        }

        virtual int handlePacket(JDPacket* p)
        {
            return other.handlePacket(p);
        }
    };

    /**
     * This class represents the logic driver, which is consistent across all JACDAC devices.
     *
     * It handles addressing and the routing of control packets from the bus to their respective drivers.
     **/
    class JDLogicDriver : public JDDriver
    {
        JDControlPacket* rxControlPacket; // given to drivers upon receiving a control packet from another device.
        JDPacket* txControlPacket; // used to transmit this devices' information (more optimal than repeat allocing)

        // this array is used to filter paired driver packets from consuming unneccessary processing cycles
        // on jacdac devices.
        uint8_t address_filters[JD_LOGIC_DRIVER_MAX_FILTERS];

        /**
         * A simple function to remove some code duplication, fills a given control packet(cp)
         * based upon a driver.
         *
         * @param driver the driver whose information will fill the control packet.
         *
         * @param info the allocated driver info struct (embedded inside a control packet) to fill.
         *
         * @param bytesRemaining the remaining data available for the driver to add additional payload to.
         **/
        int populateDriverInfo(JDDriver* driver, JDDriverInfo* info, uint8_t bytesRemaining);

        /**
         * This member function periodically iterates across all drivers and performs various actions. It handles the sending
         * of control packets, address assignments for local drivers, and the connection and disconnection of drivers as they
         * are added or removed from the bus.
         **/
        void timerCallback(Event);

        public:

        /**
         * Constructor.
         *
         * Creates a local initialised driver and adds itself to the driver array.
         **/
        JDLogicDriver();

        /**
         * Overrided for future use. It might be useful to control the behaviour of the logic driver in the future.
         **/
        virtual int handleControlPacket(JDControlPacket* p);

        /**
         * Called by the JDProtocol when a data packet has address 0.
         *
         * Packets addressed to zero will always be control packets, this function then iterates over all drivers
         * routing control packets correctly. Virtual drivers are populated if a packet is not handled by an existing driver.
         *
         * @param p the packet from the serial bus.
         **/
        virtual int handlePacket(JDPacket* p);

        /**
         * This function provides the ability to ignore specific packets. For instance,
         * we are not interested in packets that are paired to other devices hence we
         * shouldn't incur the processing cost.
         *
         * Used by JDProtocol for standard packets. The address 0 can never be filtered.
         *
         * @param address the address to check in the filter.
         *
         * @return true if the packet should be filtered, false if it should pass through.
         **/
        virtual bool filterPacket(uint8_t address);

        /**
         * This function adds an address to the list of filtered address.
         *
         * @param address the address to add to the filter.
         *
         * @return DEVICE_OK on success.
         **/
        int addToFilter(uint8_t address);

        /**
         * This function removes an address to the list of filtered address.
         *
         * @param address the address to remove from the filter.
         *
         * @return DEVICE_OK on success.
         **/
        int removeFromFilter(uint8_t address);
    };

    /**
     * This class handles packets produced by the JACDAC physical layer and passes them to our high level drivers.
     **/
    class JDProtocol : public CodalComponent
    {
        friend class JDLogicDriver;

        /**
         * Invoked by JACDAC when a packet is received from the serial bus.
         *
         * This handler is invoked in standard conext. This also means that users can stop the device from handling packets.
         * (which might be a bad thing).
         *
         * This handler continues to pull packets from the queue and iterate over drivers.
         **/
        void onPacketReceived(Event);

        // An instance of our logic driver
        JDLogicDriver logic;

        // A pointer to a bridge driver (if set, defaults to NULL).
        JDDriver* bridge;

        public:

        // this array holds all drivers on the device
        static JDDriver* drivers[JD_PROTOCOL_DRIVER_ARRAY_SIZE];

        // a reference to a JACDAC instance
        JACDAC& bus;

        // a singleton pointer to the current instance of JDProtocol.
        static JDProtocol* instance;

        /**
         * Constructor
         *
         * @param JD A reference to JACDAC for communicators
         *
         * @param id for the message bus, defaults to  DEVICE_ID_JACDAC_PROTOCOL
         **/
        JDProtocol(JACDAC& JD, uint16_t id = DEVICE_ID_JACDAC_PROTOCOL);

        /**
         * Sets the bridge member variable to the given JDDriver pointer.
         *
         * Bridge drivers are given all packets received on the bus, the idea being that
         * packets can be bridged to another networking medium, i.e. packet radio.
         *
         * @param bridge the driver to forward all packets to another networking medium
         *        this driver will receive all packets via the handlePacket call. If NULL
         *        is given, the bridge member variable is cleared.
         *
         * @note one limitation is that the bridge driver does not receive packets over the radio itself.
         *       Ultimately the bridge should punt packets back intro JDProtocol for correct handling.
         **/
        int setBridge(JDDriver* bridge);

        /**
         * Set the name to use for error codes and panics
         *
         * @param s the name to use for error codes and panic's
         *
         * @note Must be 6 characters or smaller.
         **/
        static int setDebugName(ManagedString s);

        /**
         * Retrieve the name used for error codes and panics
         *
         * @return the name used for error codes and panics
         **/
        static ManagedString getDebugName();

        /**
         * Adds a driver to the drivers array. The logic driver iterates over this array.
         *
         * @param device a reference to the driver to add.
         *
         * @return DEVICE_OK on success.
         **/
        virtual int add(JDDriver& device);

        /**
         * removes a driver from the drivers array. The logic driver iterates over this array.
         *
         * @param device a reference to the driver to remove.
         *
         * @return DEVICE_OK on success.
         **/
        virtual int remove(JDDriver& device);

        /**
         * A static method to send an entire, premade JDPacket on the bus. Used by the logic driver.
         *
         * @param pkt the packet to send.
         *
         * @return DEVICE_OK on success.
         **/
        static int send(JDPacket* pkt);

        /**
         * A static method to send a buffer on the bus. The buffer is placed in a JDPacket and sent.
         *
         * @param buf a pointer to the data to send
         *
         * @param len the length of the buffer
         *
         * @param address the address to use when sending the packet
         *
         * @return DEVICE_OK on success.
         **/
        static int send(uint8_t* buf, int len, uint8_t address);

        /**
         * Logs the current state of JACDAC, drivers, and the jackrouter (if provided).
         *
         * @param jr The jack router in use.
         **/
        void logState(JackRouter* jr = NULL);
    };

} // namespace codal

#endif

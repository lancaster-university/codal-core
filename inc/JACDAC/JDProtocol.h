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

// BEGIN    JD SERIAL SERVICE FLAGS
#define JD_SERVICE_EVT_CONNECTED         65520
#define JD_SERVICE_EVT_DISCONNECTED      65521
#define JD_SERVICE_EVT_PAIRED            65522
#define JD_SERVICE_EVT_UNPAIRED          65523
#define JD_SERVICE_EVT_PAIR_REJECTED     65524
#define JD_SERVICE_EVT_PAIRING_RESPONSE  65525
#define JD_SERVICE_EVT_ERROR             65526

#define JD_SERVICE_STATE_FLAGS_HOST            0x8000 // on the board
#define JD_SERVICE_STATE_FLAGS_CLIENT          0x4000 // off the board

// following flags combined with the above to yield different behaviours
#define JD_SERVICE_STATE_FLAGS_BROADCAST       0x2000 // receive all class packets regardless of the address
#define JD_SERVICE_STATE_FLAGS_PAIR            0x1000 // this flag indicates that the service should pair with another

#define JD_SERVICE_STATE_SERVICE_MODE_MSK       0xF000 // the top byte represents the current service mode.
// end combo flags

#define JD_SERVICE_STATE_FLAGS_PAIRABLE        0x0800 // this flag indicates that a service is paired with another
#define JD_SERVICE_STATE_FLAGS_PAIRED          0x0400 // this flag indicates that a service is paired with another
#define JD_SERVICE_STATE_FLAGS_PAIRING         0x0200 // this flag indicates that a service is paired with another

#define JD_SERVICE_STATE_FLAGS_INITIALISED     0x0100 // device service is running
#define JD_SERVICE_STATE_FLAGS_INITIALISING    0x0080 // a flag to indicate that a control packet has been queued
#define JD_SERVICE_STATE_FLAGS_CP_SEEN         0x0040 // indicates whether a control packet has been seen recently.

#define JD_SERVICE_STATE_COMM_RATE_MSK         0x0030 // these bits indicate the current comm rate of the service
#define JD_SERVICE_STATE_COMM_RATE_POS         16     // the position of the comm rate msk in the flags field.

#define JD_SERVICE_STATE_ERROR_MSK             0x000F // the lower 4 bits are reserved for well known errors, these are
                                               // automatically placed into control packets by the logic service.
// END      JD SERIAL SERVICE FLAGS


// BEGIN    CONTROL SERVICE FLAGS
#define JD_CONTROL_SERVICE_MAX_FILTERS                     20
#define JD_CONTROL_SERVICE_EVT_CHANGED                     2
#define JD_CONTROL_SERVICE_EVT_TIMER_CALLBACK              3

#define JD_DEVICE_FLAGS_RESERVED                  0x80
#define JD_DEVICE_FLAGS_PAIRING_MODE              0x40 // in pairing mode, control packets aren't forwarded to services
#define JD_DEVICE_FLAGS_PAIRABLE                  0x20 // advertises that a service can be optionally paired with another
#define JD_DEVICE_FLAGS_PAIRED                    0x10 // advertises that a service is already paired with another.

#define JD_DEVICE_FLAGS_CONFLICT                        0x08
#define JD_DEVICE_FLAGS_UNCERTAIN                       0x04
#define JD_DEVICE_FLAGS_NACK                            0x02
#define JD_DEVICE_FLAGS_ACK                             0x01

#define JD_SERVICE_INFO_TYPE_HELLO                       0x1
#define JD_SERVICE_INFO_TYPE_PAIRING_REQUEST             0x2
#define JD_SERVICE_INFO_TYPE_ERROR                       0x3
#define JD_SERVICE_INFO_TYPE_PANIC                       0xF

#define JD_SERVICE_INFO_MAX_PAYLOAD_SIZE                 16

// END      CONTROL SERVICE FLAGS


// BEGIN    JD SERIAL PROTOCOL

#include "JDClasses.h"

#define JD_PROTOCOL_SERVICE_ARRAY_SIZE                  20
#define JD_MAX_DEVICE_NAME_LENGTH                       8

#define JD_SERVICE_INFO_HEADER_SIZE                     6
#define JD_CONTROL_PACKET_HEADER_SIZE                   10

#define JD_MAX_PACKET_SIZE                              255

namespace codal
{
    class JDProtocol;

    /**
     * This struct represents a JDControlPacket used by the logic service
     * A control packet provides full information about a service, it's most important use is to translates the address used in
     * standard packets to the full service information. Standard packet address == control packet address.
     *
     * Currently there are two types of packet:
     * CONTROL_JD_TYPE_HELLO - Which broadcasts the availablity of a service
     * CONTROL_JD_TYPE_PAIRING_REQUEST - Used when services are pairing to one another.
     **/
    struct JDControlPacket
    {
        uint64_t serial_number; // the "unique" serial number of the device.
        uint8_t device_address;
        uint8_t device_flags;
        uint8_t data[];
    } __attribute((__packed__));

    struct JDServiceInfo
    {
        uint8_t service_flags;
        uint8_t service_status: 4, advertisement_size: 4; // error code upper four bits, then advertisement size lower four bits
        uint32_t service_class;  // the class of the service
        uint8_t data[]; // optional additional data, maximum of 16 bytes
    } __attribute((__packed__));

    // This enumeration specifies that supported configurations that services should utilise.
    // Many combinations of flags are supported, but only the ones listed here have been fully implemented.
    enum ServiceType
    {
        ClientService = JD_SERVICE_STATE_FLAGS_CLIENT, // the service is seeking the use of another device's resource
        PairedService = JD_SERVICE_STATE_FLAGS_BROADCAST | JD_SERVICE_STATE_FLAGS_PAIR,
        HostService = JD_SERVICE_STATE_FLAGS_HOST, // the service is hosting a resource for others to use.
        PairableHostService = JD_SERVICE_STATE_FLAGS_PAIRABLE | JD_SERVICE_STATE_FLAGS_HOST, // the service is allowed to pair with another service of the same class
        BroadcastHostService = JD_SERVICE_STATE_FLAGS_HOST | JD_SERVICE_STATE_FLAGS_BROADCAST, // the service is enumerated with its own address, and receives all packets of the same class (including control packets)
        BroadcastClientService = JD_SERVICE_STATE_FLAGS_CLIENT | JD_SERVICE_STATE_FLAGS_BROADCAST, // the service is not enumerated, and receives all packets of the same class (including control packets)
    };

    enum ServiceErrorCode
    {
        // No error occurred.
        SERVICE_OK = 0,

        // Device calibration information
        SERVICE_CALIBRATION_IN_PROGRESS,
        SERVICE_CALIBRATION_REQUIRED,

        // The service has run out of some essential resource (e.g. allocated memory)
        SERVICE_NO_RESOURCES,

        // The service operation could not be performed as some essential resource is busy (e.g. the display)
        SERVICE_BUSY,

        // I2C / SPI Communication error occured
        SERVICE_COMMS_ERROR,

        // An invalid state was detected (i.e. not initialised)
        SERVICE_INVALID_STATE,

        // an external peripheral has a malfunction e.g. external circuitry is drawing too much power.
        SERVICE_PERIPHERAL_MALFUNCTION
    };

    /**
     * This struct represents a JDServiceState used by a JDService.
     *
     * It is perhaps named incorrectly, but JDServiceState represents the core information about the service which is placed into control packets.
     * A rolling counter is used to trigger control packets and other core service events.
     **/
    struct JDServiceState
    {
        uint8_t address; // the address assigned by the logic service.
        uint8_t rolling_counter; // used to trigger various time related events
        uint16_t flags; // various flags indicating the state of the service
        uint64_t serial_number; // the serial number used to "uniquely" identify a device
        uint32_t service_class; // the class of the service, created or selected from the list in JDClasses.h
        uint8_t service_number;

        /**
         * Constructor, creates a local service using just the service class.
         *
         * Should be used if only a local service is required.
         *
         * @param service_class the class of the service listed in JDClasses.h
         **/
        JDServiceState(uint32_t service_class)
        {
            this->address = 0;
            this->rolling_counter = 0;
            this->flags = JD_SERVICE_STATE_FLAGS_HOST;
            this->serial_number = target_get_serial();
            this->service_class = service_class;
            this->service_number = 255;
        }

        /**
         * Constructor, creates a service given a ServiceType (enumeration above) and the service class.
         *
         * Should be used if you need to use any of the other types from the enumeration (most of the time, this will be used).
         *
         * @param t the service type to use
         *
         * @param service_class the class of the service listed in JDClasses.h
         *
         * @note the VirtualService will always have a serial_number of 0 by default, as the serial number is used as a filter.
         *       if a filter is required, then the full constructor should be used (below).
         **/
        JDServiceState(ServiceType t, uint32_t service_class)
        {
            this->address = 0;
            this->rolling_counter = 0;
            this->flags |= t;

            if (t & JD_SERVICE_STATE_FLAGS_CLIENT)
                this->serial_number = 0;
            else
                this->serial_number = target_get_serial();

            this->service_class = service_class;
            this->service_number = 255;
        }

        /**
         * Constructor, allows (almost) full specification of a JDServiceState.
         *
         * Should be used if you need to specify all of the fields, i.e. if you're adding complex logic that requires already
         * initialised services.
         *
         * @param a the address of the service
         *
         * @param flags the low-level flags that are normally set by using the ServiceType enum.
         *
         * @param serial_number the serial number of the service
         *
         * @param service_class the class of the service listed in JDClasses.h
         *
         * @note you are responsible for any weirdness you achieve using this constructor.
         **/
        JDServiceState(uint8_t address, uint8_t service_number, uint16_t flags, uint64_t serial_number, uint32_t service_class)
        {
            this->address = address;
            this->rolling_counter = 0;
            this->flags = flags;
            this->serial_number = serial_number;
            this->service_class = service_class;
            this->service_number = service_number;
        }

        /**
         * Returns the communication rate of this service.
         **/
        JDBaudRate getBaudRate()
        {
            uint32_t r = ((this->flags & JD_SERVICE_STATE_COMM_RATE_MSK) >> JD_SERVICE_STATE_COMM_RATE_POS) + 1;
            return (JDBaudRate)r;
        }

        /**
         * Returns the communication rate of this service.
         **/
        void setBaudRate(JDBaudRate br)
        {
            this->flags &= ~JD_SERVICE_STATE_COMM_RATE_MSK;
            // JDBaudRate values start from one, we only use 3 bits in our flags field for the comm rate... subtract one
            this->flags |= ((uint8_t)br - 1) << JD_SERVICE_STATE_COMM_RATE_POS;
        }

        /**
         * Sets the mode to the given ServiceType
         *
         * @param m the new mode the service should move to.
         *
         * @param initialised whether the service is initialised or not (defaults to false).
         **/
        void setMode(ServiceType m, bool initialised = false)
        {
            this->flags &= ~JD_SERVICE_STATE_SERVICE_MODE_MSK;
            this->flags |= m;

            if (initialised)
                this->flags |= JD_SERVICE_STATE_FLAGS_INITIALISED;
            else
                this->flags &= ~JD_SERVICE_STATE_FLAGS_INITIALISED;
        }

        /**
         * Sets the error portion of flags to the given error code
         *
         * @param e the error code to place into control packets
         **/
        void setError(ServiceErrorCode e)
        {
            uint32_t flags = this->flags & ~(JD_SERVICE_STATE_ERROR_MSK);
            this->flags = flags | (uint8_t) e;
        }

        /**
         * Retrieves the current error code from the error portion of flags.
         *
         * @return a JDServiceStateErrorCode
         **/
        ServiceErrorCode getError()
        {
            return (ServiceErrorCode)(this->flags & JD_SERVICE_STATE_ERROR_MSK);
        }

        /**
         * Used to determine what mode the service is currently in.
         *
         * This will check to see if the flags field resembles the VirtualService mode specified in the ServiceType enumeration.
         *
         * @returns true if in VirtualService mode.
         **/
        bool isClient()
        {
            return (this->flags & JD_SERVICE_STATE_FLAGS_CLIENT) && !(this->flags & JD_SERVICE_STATE_FLAGS_BROADCAST);
        }

        /**
         * Used to determine what mode the service is currently in.
         *
         * This will check to see if the flags field resembles the PairedService mode specified in the ServiceType enumeration.
         *
         * @returns true if in PairedService mode.
         **/
        bool isPairedToDevice()
        {
            #warning remove this
            return this->flags & JD_SERVICE_STATE_FLAGS_BROADCAST && this->flags & JD_SERVICE_STATE_FLAGS_PAIR;
        }

        /**
         * Used to determine what mode the service is currently in.
         *
         * This will check to see if the flags field resembles the HostService mode specified in the ServiceType enumeration.
         *
         * @returns true if in SnifferService mode.
         **/
        bool isHost()
        {
            return this->flags & JD_SERVICE_STATE_FLAGS_HOST && !(this->flags & JD_SERVICE_STATE_FLAGS_BROADCAST);
        }

        /**
         * Used to determine what mode the service is currently in.
         *
         * This will check to see if the flags field resembles the BroadcastService mode specified in the ServiceType enumeration.
         *
         * @returns true if in BroadcastService mode.
         **/
        bool isBroadcastHost()
        {
            return this->flags & JD_SERVICE_STATE_FLAGS_HOST && this->flags & JD_SERVICE_STATE_FLAGS_BROADCAST;
        }

        /**
         * Used to determine what mode the service is currently in.
         *
         * This will check to see if the flags field resembles the SnifferService mode specified in the ServiceType enumeration.
         *
         * @returns true if in SnifferService mode.
         **/
        bool isBroadcastClient()
        {
            return this->flags & JD_SERVICE_STATE_FLAGS_CLIENT && this->flags & JD_SERVICE_STATE_FLAGS_BROADCAST;
        }

        /**
         * Indicates if the service is currently paired to another.
         *
         * @returns true if paired
         **/
        bool isPaired()
        {
            return this->flags & JD_SERVICE_STATE_FLAGS_PAIRED;
        }

        /**
         * Indicates if the service can be currently paired to another.
         *
         * @returns true if pairable
         **/
        bool isPairable()
        {
            return this->flags & JD_SERVICE_STATE_FLAGS_PAIRABLE;
        }

        /**
         * Indicates if the service is currently in the process of pairing to another.
         *
         * @returns true if pairing
         **/
        bool isPairing()
        {
            return this->flags & JD_SERVICE_STATE_FLAGS_PAIRING;
        }
    };

    class JDPairedService;
    class JDProtocol;

    /**
     * This class presents a common abstraction for all JDServices. It also contains some default member functions to perform common operations.
     * This should be subclassed by any service implementation
     **/
    class JDService : public CodalComponent
    {
        friend class JDControlService;
        friend class JDProtocol;
        friend class JDBroadcastMap;
        // the above need direct access to our member variables and more.

        /**
         * After calling sendPairingPacket, this member function is called when the device is enumerated.
         *
         * It then creates a pairing control packet, and sends it to the remote instance.
         **/
        void pair();

        protected:

        // Due to the dynamic nature of JACDAC when a new service is created, this variable is incremented.
        // JACDAC id's are allocated from 3000 - 4000
        static uint32_t dynamicId;

        // When we pair to another service, this points to the stub of our partner.
        JDPairedService* pairedInstance;

        // A struct the represents the state of the service.
        JDServiceState state;

        /**
         * This method internally redirects specific packets from the control service.
         *
         * i.e. it switches the type of the logic packet, and redirects it to handleControlPacket or handlePairingPacket accordingly.
         **/
        int handleLogicPacket(JDControlPacket* cp);

        /**
         * Called by the logic service when a new state is connected to the serial bus
         *
         * @param state an instance of JDServiceState representing the device that has been connected
         *
         * @return SERVICE_STATE_OK for success
         **/
        virtual int deviceConnected(JDServiceState state);

        /**
         * Called by the logic service when this service has been disconnected from the serial bus.
         *
         * This is only called if a service is in VirtualMode and the virtualised device disappears from the bus.
         *
         * @return SERVICE_STATE_OK for success
         **/
        virtual int deviceRemoved();

        /**
         * This should be called when a service wishes to pair with another. A service should first detect a service in pairing mode
         * by observing packets in Broadcast mode. PairedService from the ServiceType enumeration first starts in broadcast mode only,
         * observes packets looking for a device to pair with. When a pairable device appears, the service enumerates, and sends a
         * pairing packet by calling this member function.
         *
         * @param d the device to pair too.
         *
         * @returns SERVICE_STATE_OK on success.
         **/
        virtual int sendPairingPacket(JDServiceState d);

        /**
         * This is called when a paired service is removed from the bus. It unpairs this service instance, and fires an event
         * using the services id, and the event code JD_SERVICE_EVT_UNPAIRED.
         **/
        void partnerDisconnected(Event);

        /**
         * A convenience function that calls JACDAC->send with parameters supplied from this instances' JDServiceState
         *
         * @param buf the data to send
         * @param len the length of the data.
         *
         * @return SERVICE_STATE_OK on success.
         **/
        int send(uint8_t* buf, int len);

        public:

        /**
         * Constructor
         *
         * @param d a struct containing a device representation, see JDServiceState.
         * */
        JDService(JDServiceState d);

        /**
         * Invoked by the logic service when it is queuing a control packet.
         *
         * This allows the addition of service specific control packet information and the setting of any additional flags.
         *
         * @param p A pointer to the packet, where the data field contains a pre-filled control packet.
         *
         * @return SERVICE_STATE_OK on success
         **/
        virtual int populateServiceInfo(JDServiceInfo* info, uint8_t bytesRemaining);

        /**
         * Invoked by the logic service when a control packet with the address of the service is received.
         *
         * Control packets are routed by address, or by class in broadcast mode. Services
         * can override this function to handle additional payloads in control packet.s
         *
         * @param p the packet from the serial bus. Services should cast p->data to a JDControlPacket.
         *
         * @return SERVICE_STATE_OK to signal that the packet has been handled, or SERVICE_STATE_CANCELLED to indicate the logic service
         *         should continue to search for a service.
         **/
        virtual int handleControlPacket(JDControlPacket* info);

        /**
         * Invoked by the logic service when a control packet with its type set to error is received.
         *
         *
         * @param p the packet from the serial bus. Services should cast p->data to a JDControlPacket,
         *          then JDControlPacket->data to ControlPacketError to obtain the error code.
         *
         * @return SERVICE_STATE_OK to signal that the packet has been handled, or SERVICE_STATE_CANCELLED to indicate the logic service
         *         should continue to search for a service.
         **/
        virtual int handleErrorPacket(JDControlPacket* info);

        /**
         * Invoked by the logic service when a pairing packet with the address of the service is received.
         *
         * Pairing packets are Control packets with the type set to CONTROL_JD_TYPE_PAIRING_REQUEST. They are routed by
         * address, or by class in broadcast mode. Services can override this function to handle additional payloads in
         * control packet.
         *
         * Pairing packets contain the source information of the device that sent the pairing request in cp->data;
         *
         * @param p the packet from the serial bus. Services should cast p->data to a JDControlPacket.
         *
         * @return SERVICE_STATE_OK to signal that the packet has been handled, or SERVICE_STATE_CANCELLED to indicate the logic service
         *         should continue to search for a service.
         **/
        virtual int handlePairingPacket(JDControlPacket* p);

        /**
         * Invoked by the Protocol service when a standard packet with the address of the service is received.
         *
         * @param p the packet from the serial bus. Services should cast p->data to their agreed upon structure..
         *
         * @return SERVICE_STATE_OK to signal that the packet has been handled, or SERVICE_STATE_CANCELLED to indicate the logic service
         *         should continue to search for a service.
         **/
        virtual int handlePacket(JDPacket* p);

        /**
         * Returns the current connected state of this service instance.
         *
         * @return true for connected, false for disconnected
         **/
        virtual bool isConnected();

        /**
         * Returns the current pairing state of this service instance.
         *
         * @return true for paired, false for unpaired
         **/
        virtual bool isPaired();

        /**
         * Returns whether the service is advertising a pairable state
         *
         * @return true for paired, false for unpaired
         **/
        virtual bool isPairable();

        /**
         * Retrieves the address of the service.
         *
         * @return the address.
         **/
        uint8_t getAddress();

        /**
         * Retrieves the class of the service.
         *
         * @return the class.
         **/
        uint32_t getClass();

        /**
         * Retrieves the state of the service.
         *
         * @return the internal service state.
         **/
        JDServiceState getState();

        /**
         * Retrieves the serial number in use by this service.
         *
         * @return the serial number
         **/
        uint32_t getSerialNumber();

        /**
         * Destructor, removes this service from the services array and deletes the pairedInstance member variable if allocated.
         **/
        ~JDService();
    };

    /**
     * This class is a stub of a remote service that a local service is paired with.
     *
     * It simply forwards all standard packets to the paired local service for processing.
     **/
    class JDPairedService : public JDService
    {
        JDService& other;

        public:

        JDPairedService(JDServiceState d, JDService& other) : JDService(d), other(other)
        {
        }

        virtual int handlePacket(JDPacket* p)
        {
            return other.handlePacket(p);
        }
    };

    /**
     * This class represents the logic service, which is consistent across all JACDAC devices.
     *
     * It handles addressing and the routing of control packets from the bus to their respective services.
     **/
    class JDControlService : public JDService
    {
        JDControlPacket* rxControlPacket; // given to services upon receiving a control packet from another device.
        JDPacket* txControlPacket; // used to transmit this devices' information (more optimal than repeat allocing)

        // this array is used to filter paired service packets from consuming unneccessary processing cycles
        // on jacdac devices.
        uint8_t address_filters[JD_CONTROL_SERVICE_MAX_FILTERS];

        /**
         * A simple function to remove some code duplication, fills a given control packet(cp)
         * based upon a service.
         *
         * @param service the service whose information will fill the control packet.
         *
         * @param info the allocated service info struct (embedded inside a control packet) to fill.
         *
         * @param bytesRemaining the remaining data available for the service to add additional payload to.
         **/
        int populateServiceInfo(JDService* service, JDServiceInfo* info, uint8_t bytesRemaining);

        /**
         * This member function periodically iterates across all services and performs various actions. It handles the sending
         * of control packets, address assignments for local services, and the connection and disconnection of services as they
         * are added or removed from the bus.
         **/
        void timerCallback(Event);

        public:

        /**
         * Constructor.
         *
         * Creates a local initialised service and adds itself to the service array.
         **/
        JDControlService();

        /**
         * Overrided for future use. It might be useful to control the behaviour of the logic service in the future.
         **/
        virtual int handleControlPacket(JDControlPacket* p);

        /**
         * Called by the JDProtocol when a data packet has address 0.
         *
         * Packets addressed to zero will always be control packets, this function then iterates over all services
         * routing control packets correctly. Virtual services are populated if a packet is not handled by an existing service.
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
         * @return SERVICE_STATE_OK on success.
         **/
        int addToFilter(uint8_t address);

        /**
         * This function removes an address to the list of filtered address.
         *
         * @param address the address to remove from the filter.
         *
         * @return SERVICE_STATE_OK on success.
         **/
        int removeFromFilter(uint8_t address);
    };

    /**
     * This class handles packets produced by the JACDAC physical layer and passes them to our high level services.
     **/
    class JDProtocol : public CodalComponent
    {
        friend class JDControlService;

        /**
         * Invoked by JACDAC when a packet is received from the serial bus.
         *
         * This handler is invoked in standard conext. This also means that users can stop the device from handling packets.
         * (which might be a bad thing).
         *
         * This handler continues to pull packets from the queue and iterate over services.
         **/
        void onPacketReceived(Event);

        // An instance of our logic service
        JDControlService control;

        // A pointer to a bridge service (if set, defaults to NULL).
        JDService* bridge;

        public:

        // this array holds all services on the device
        static JDService* services[JD_PROTOCOL_SERVICE_ARRAY_SIZE];

        // a reference to a JACDAC instance
        JACDAC& bus;

        // a singleton pointer to the current instance of JDProtocol.
        static JDProtocol* instance;

        /**
         * Constructor
         *
         * @param JD A reference to JACDAC for communicators
         *
         * @param id for the message bus, defaults to  SERVICE_STATE_ID_JACDAC_PROTOCOL
         **/
        JDProtocol(JACDAC& JD, ManagedString deviceName = ManagedString(), uint16_t id = DEVICE_ID_JACDAC_PROTOCOL);

        /**
         * Sets the bridge member variable to the given JDService pointer.
         *
         * Bridge services are given all packets received on the bus, the idea being that
         * packets can be bridged to another networking medium, i.e. packet radio.
         *
         * @param bridge the service to forward all packets to another networking medium
         *        this service will receive all packets via the handlePacket call. If NULL
         *        is given, the bridge member variable is cleared.
         *
         * @note one limitation is that the bridge service does not receive packets over the radio itself.
         *       Ultimately the bridge should punt packets back intro JDProtocol for correct handling.
         **/
        int setBridge(JDService* bridge);

        /**
         * Set the name to use for error codes and panics
         *
         * @param s the name to use for error codes and panic's
         *
         * @note Must be 6 characters or smaller.
         **/
        static int setDeviceName(ManagedString s);

        /**
         * Retrieve the name used for error codes and panics
         *
         * @return the name used for error codes and panics
         **/
        static ManagedString getDeviceName();

        /**
         * Adds a service to the services array. The logic service iterates over this array.
         *
         * @param device a reference to the service to add.
         *
         * @return SERVICE_STATE_OK on success.
         **/
        virtual int add(JDService& device);

        /**
         * removes a service from the services array. The logic service iterates over this array.
         *
         * @param device a reference to the service to remove.
         *
         * @return SERVICE_STATE_OK on success.
         **/
        virtual int remove(JDService& device);

        /**
         * A static method to send an entire, premade JDPacket on the bus. Used by the logic service.
         *
         * @param pkt the packet to send.
         *
         * @return SERVICE_STATE_OK on success.
         **/
        static int send(JDPacket* pkt);

        /**
         * Logs the current state of JACDAC, services, and the jackrouter (if provided).
         *
         * @param jr The jack router in use.
         **/
        void logState(JackRouter* jr = NULL);
    };

} // namespace codal

#endif

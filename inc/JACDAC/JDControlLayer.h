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

// #define JD_SERVICE_STATE_FLAGS_PAIRABLE        0x0800 // this flag indicates that a service is paired with another
// #define JD_SERVICE_STATE_FLAGS_PAIRED          0x0400 // this flag indicates that a service is paired with another
// #define JD_SERVICE_STATE_FLAGS_PAIRING         0x0200 // this flag indicates that a service is paired with another

#define JD_SERVICE_STATUS_FLAGS_INITIALISED     0x01 // device service is running
// END      JD SERIAL SERVICE FLAGS


// BEGIN    CONTROL SERVICE FLAGS
#define JD_CONTROL_SERVICE_STATUS_ENUMERATE                 0x02
#define JD_CONTROL_SERVICE_STATUS_ENUMERATING               0x04
#define JD_CONTROL_SERVICE_STATUS_ENUMERATED                0x08

#define JD_CONTROL_SERVICE_EVT_CHANGED                      2
#define JD_CONTROL_SERVICE_EVT_TIMER_CALLBACK               3

// pairing mode comes later... ;)
// #define JD_DEVICE_FLAGS_RESERVED                        0x80
// #define JD_DEVICE_FLAGS_PAIRING_MODE                    0x40 // in pairing mode, control packets aren't forwarded to services
// #define JD_DEVICE_FLAGS_PAIRABLE                        0x20 // advertises that a service can be optionally paired with another
// #define JD_DEVICE_FLAGS_PAIRED                          0x10 // advertises that a service is already paired with another.

#define JD_DEVICE_FLAGS_NACK                            0x08
#define JD_DEVICE_FLAGS_HAS_NAME                        0x04
#define JD_DEVICE_FLAGS_PROPOSING                       0x02
#define JD_DEVICE_FLAGS_REJECT                          0x01

#define JD_SERVICE_INFO_MAX_PAYLOAD_SIZE                 16

// END      CONTROL SERVICE FLAGS


// BEGIN    JD SERIAL PROTOCOL

#include "JDClasses.h"

#define JD_PROTOCOL_SERVICE_ARRAY_SIZE                  20
#define JD_MAX_DEVICE_NAME_LENGTH                       8
#define JD_MAX_HOST_SERVICES                            16

#define JD_SERVICE_INFO_HEADER_SIZE                     6
#define JD_CONTROL_PACKET_HEADER_SIZE                   10

#define JD_MAX_PACKET_SIZE                              255

namespace codal
{
    class JDControlLayer;

    // This enumeration specifies that supported configurations that services should utilise.
    // Many combinations of flags are supported, but only the ones listed here have been fully implemented.
    enum ServiceMode
    {
        ClientService = JD_SERVICE_STATE_FLAGS_CLIENT, // the service is seeking the use of another device's resource
        HostService = JD_SERVICE_STATE_FLAGS_HOST, // the service is hosting a resource for others to use.
        BroadcastHostService = JD_SERVICE_STATE_FLAGS_HOST | JD_SERVICE_STATE_FLAGS_BROADCAST, // the service is enumerated with its own address, and receives all packets of the same class (including control packets)
    };

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
        uint64_t udid; // the "unique" serial number of the device.
        uint8_t device_address;
        uint8_t device_flags;
        uint8_t data[];
    } __attribute((__packed__));

    struct JDServiceInformation
    {
        uint32_t service_class;  // the class of the service
        uint8_t service_flags;
        uint8_t advertisement_size;
        uint8_t data[]; // optional additional data, maximum of 16 bytes
    } __attribute((__packed__));

    struct JDDevice
    {
        uint64_t udid;
        uint8_t device_flags;
        uint8_t device_address;
        uint8_t communication_rate;
        uint8_t rolling_counter;
        uint8_t broadcast_servicemap[JD_MAX_HOST_SERVICES / 2]; // use to map remote broadcast services to local broadcast services.
        JDDevice* next;
        uint8_t* name;
    };

    class JDControlLayer;

    /**
     * This class presents a common abstraction for all JDServices. It also contains some default member functions to perform common operations.
     * This should be subclassed by any service implementation
     **/
    class JDService : public CodalComponent
    {
        friend class JDControlService;
        friend class JDControlLayer;
        // the above need direct access to our member variables and more

        ServiceMode mode;
        JDDevice* device;
        uint32_t service_class;

        protected:

        // Due to the dynamic nature of JACDAC when a new service is created, this variable is incremented.
        // JACDAC id's are allocated from 3000 - 4000
        static uint32_t dynamicId;

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
        virtual int hostConnected(JDServiceState state);

        /**
         * Called by the logic service when this service has been disconnected from the serial bus.
         *
         * This is only called if a service is in VirtualMode and the virtualised device disappears from the bus.
         *
         * @return SERVICE_STATE_OK for success
         **/
        virtual int hostDisconnected();

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
         * */
        JDService(uint32_t serviceClass, ServiceMode m);

        /**
         * Invoked by the logic service when it is queuing a control packet.
         *
         * This allows the addition of service specific control packet information and the setting of any additional flags.
         *
         * @param p A pointer to the packet, where the data field contains a pre-filled control packet.
         *
         * @return SERVICE_STATE_OK on success
         **/
        virtual int addAdvertisementData(JDServiceInfo* info);

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
         * Retrieves the address of the service.
         *
         * @return the address.
         **/
        JDDevice* getDevice();

        /**
         * Retrieves the class of the service.
         *
         * @return the class.
         **/
        uint32_t getServiceClass();

        /**
         * Destructor, removes this service from the services array and deletes the pairedInstance member variable if allocated.
         **/
        ~JDService();
    };

    /**
     * This class represents the logic service, which is consistent across all JACDAC devices.
     *
     * It handles addressing and the routing of control packets from the bus to their respective services.
     **/
    class JDControlService : public JDService
    {
        JDDevice* remoteDevices;
        JDDevice* controller;
        JDControlPacket* enumerationData;

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
         * Called by the JDControlLayer when a data packet has address 0.
         *
         * Packets addressed to zero will always be control packets, this function then iterates over all services
         * routing control packets correctly. Virtual services are populated if a packet is not handled by an existing service.
         *
         * @param p the packet from the serial bus.
         **/
        virtual int handlePacket(JDPacket* p);

        /**
         *
         **/
        int enumerate();

        /**
         *
         **/
        bool isEnumerated();

        /**
         *
         **/
        bool isEnumerating();

        /**
         *
         **/
        int disconnect();
    };

    /**
     * This class handles packets produced by the JACDAC physical layer and passes them to our high level services.
     **/
    class JDControlLayer : public CodalComponent
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

        // a singleton pointer to the current instance of JDControlLayer.
        static JDControlLayer* instance;

        /**
         * Constructor
         *
         * @param JD A reference to JACDAC for communicators
         *
         * @param id for the message bus, defaults to  SERVICE_STATE_ID_JACDAC_PROTOCOL
         **/
        JDControlLayer(JACDAC& JD, ManagedString deviceName = ManagedString(), uint16_t id = DEVICE_ID_JACDAC_PROTOCOL);

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
         *       Ultimately the bridge should punt packets back intro JDControlLayer for correct handling.
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

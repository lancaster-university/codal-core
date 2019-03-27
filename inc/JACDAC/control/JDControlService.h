#ifndef CODAL_JD_CONTROL_SERVICE_H
#define CODAL_JD_CONTROL_SERVICE_H

#include "JDService.h"
#include "JDRNGService.h"
#include "JDNamingService.h"
#include "ManagedString.h"

#define JD_CONTROL_SERVICE_STATUS_ENUMERATE                 0x02
#define JD_CONTROL_SERVICE_STATUS_ENUMERATING               0x04
#define JD_CONTROL_SERVICE_STATUS_ENUMERATED                0x08
#define JD_CONTROL_SERVICE_STATUS_BUS_LO                    0x10

#define JD_CONTROL_SERVICE_EVT_CHANGED                      2
#define JD_CONTROL_SERVICE_EVT_TIMER_CALLBACK               3

#define JD_CONTROL_PACKET_HEADER_SIZE                   10

namespace codal
{
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
        uint64_t unique_device_identifier; // the "unique" serial number of the device.
        uint8_t device_address;
        uint8_t device_flags;
        uint8_t data[];
    } __attribute((__packed__));

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

        JDDeviceManager deviceManager;
        JDRNGService rngService;
        JDNamingService namingService;

        /**
         * This member function periodically iterates across all services and performs various actions. It handles the sending
         * of control packets, address assignments for local services, and the connection and disconnection of services as they
         * are added or removed from the bus.
         **/
        void timerCallback(Event);

        void deviceDisconnected(JDDevice* device);

        void deviceEnumerated();

        int formControlPacket();

        int send(uint8_t* buf, int len) override;

        public:

        /**
         * Constructor.
         *
         * Creates a local initialised service and adds itself to the service array.
         **/
        JDControlService(ManagedString deviceName);

        /**
         * Overrided for future use. It might be useful to control the behaviour of the logic service in the future.
         **/
        virtual int handleServiceInformation(JDDevice* device, JDServiceInformation* p) override;

        /**
         * Called by the JACDAC when a data packet has address 0.
         *
         * Packets addressed to zero will always be control packets, this function then iterates over all services
         * routing control packets correctly. Virtual services are populated if a packet is not handled by an existing service.
         *
         * @param p the packet from the serial bus.
         **/
        virtual int handlePacket(JDPacket* p) override;

        /**
         *
         **/
        JDDevice* getRemoteDevice(uint8_t device_address);

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

        ManagedString getDeviceName();

        int setDeviceName(ManagedString name);

        JDDevice* getDevice(uint8_t device_address);
    };
}

#endif
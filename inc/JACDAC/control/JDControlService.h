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
     * This struct represents a JDControlPacket used by the control service residing on device_address 0
     * with service_number 0.
     *
     * A control packet provides full information about services on a device, it's most important use is to translates the address used in
     * standard packets to the full service information.
     **/
    struct JDControlPacket
    {
        uint64_t unique_device_identifier; // the "unique" serial number of the device.
        uint8_t device_address;
        uint8_t device_flags;
        uint8_t data[];
    } __attribute((__packed__));

    /**
     * This class represents the control service, which is consistent across all JACDAC devices.
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
         * This member function periodically iterates across all devices and performs various actions. It handles the sending
         * of control packets, address assignment for the device, and the connection and disconnection of services as devices
         * are added or removed from the bus.
         **/
        void timerCallback(Event);

        /**
         * This member function disconnects the services attached to the given device. It is invoked
         * when the controlService has detected the absence of a device.
         *
         * @param device the device which has been removed from the bus.
         **/
        void deviceDisconnected(JDDevice* device);

        /**
         * This member function connects the device struct used to enumerate with HostServices running
         * on the device.
         **/
        void deviceEnumerated();

        /**
         * This member function composes the control packet data field which includes service information.
         * It is responsible for calling addAdvertisementData.
         *
         * @return the size of the controlPacket.
         **/
        int formControlPacket();

        /**
         * This function overrides the default implementation of JDService->send, instead using
         * a device_address of 0, a service number of 0, and a device_unique_identifier of NULL.
         **/
        int send(uint8_t* buf, int len) override;

        public:

        /**
         * Constructor.
         *
         * Creates a local initialised service and adds itself to the service array.
         *
         * @param deviceName the device name to use on the bus.
         **/
        JDControlService(ManagedString deviceName);

        void routePacket(JDPacket* p);

        /**
         * Overridden for future use. It might be useful to control the behaviour of the logic service in the future.
         **/
        virtual int handleServiceInformation(JDDevice* device, JDServiceInformation* p) override;

        /**
         * Called by the JACDAC when a data packet has address 0.
         *
         * Packets addressed to zero will always be addressed to a control service.
         *
         * This function forwards packets for specific service_numbers (such as name and RNG services).
         *
         * It then iterates over all services routing control packets correctly. Virtual services are
         * populated if a packet is not handled by an existing service.
         *
         * @param p the packet from the serial bus.
         **/
        virtual int handlePacket(JDPacket* p) override;

        /**
         * Get a device based upon its device_address
         *
         * @param device_address the device_address of the device to retrieve
         *
         * @returns NULL if not found, or a ptr to a JDDevice struct.
         **/
        JDDevice* getRemoteDevice(uint8_t device_address);

        /**
         * Starts the enumeration process of the device only if a service has host services to
         * enumerate.
         *
         * @return DEVICE_OK on success.
         **/
        int enumerate();

        /**
         * Returns the current enumeration state.
         *
         * @returns true if enumerated, false otherwise.
         **/
        bool isEnumerated();

        /**
         * Returns a bool whose value is based on whether the device is enumerating.
         *
         * @returns true if enumerating, false otherwise.
         **/
        bool isEnumerating();

        /**
         * The opposite of enumerate, and stops Control Packets being transmitted on the bus.
         *
         * @return DEVICE_OK on success.
         **/
        int disconnect();

        /**
         * Retrieves the current device name.
         *
         * @returns the device name as a ManagedString.
         **/
        ManagedString getDeviceName();

        /**
         * Sets the current device name.
         *
         * @param name the name for the device.
         *
         * @returns DEVICE_OK on success.
         **/
        int setDeviceName(ManagedString name);
    };
}

#endif
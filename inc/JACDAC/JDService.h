#ifndef CODAL_JD_SERVICE_H
#define CODAL_JD_SERVICE_H

#include "CodalComponent.h"
#include "JDPhysicalLayer.h"
#include "JDServiceClasses.h"
#include "JDDeviceManager.h"

#define JD_MAX_HOST_SERVICES                            JD_DEVICE_MAX_HOST_SERVICES

#define JD_SERVICE_EVT_CONNECTED                        65520
#define JD_SERVICE_EVT_DISCONNECTED                     65521
#define JD_SERVICE_EVT_ERROR                            65526
#define JD_SERVICE_NUMBER_UNINITIALISED_VAL             65535  // used as the service_number when a service is not initialised
#define JD_SERVICE_STATUS_FLAGS_INITIALISED             0x02 // device service is running

#define JD_SERVICE_INFO_HEADER_SIZE                     6

#define JD_SERVICE_MODE_CLIENT                          1
#define JD_SERVICE_MODE_HOST                            2
#define JD_SERVICE_MODE_BROADCAST_HOST                  3
#define JD_SERVICE_MODE_CONTROL_LAYER                   4

namespace codal
{
    // This enumeration specifies that supported configurations that services should utilise.
    // Many combinations of flags are supported, but only the ones listed here have been fully implemented.
    enum JDServiceMode
    {
        ClientService = JD_SERVICE_MODE_CLIENT, // the service is seeking the use of another device's resource
        HostService = JD_SERVICE_MODE_HOST, // the service is hosting a resource for others to use.
        BroadcastHostService = JD_SERVICE_MODE_BROADCAST_HOST, // the service is enumerated with its own address, and receives all packets of the same class (including control packets)
        ControlLayerService = JD_SERVICE_MODE_CONTROL_LAYER // this service is not consider an enumerable service.
    };

    struct JDServiceInformation
    {
        uint32_t service_class;  // the class of the service
        uint8_t service_flags;
        uint8_t advertisement_size;
        uint8_t data[]; // optional additional data, maximum of 16 bytes
    } __attribute((__packed__));

    /**
     * This class presents a common abstraction for all JDServices. It also contains some default member functions to perform common operations.
     * This should be subclassed by any service implementation
     **/
    class JDService : public CodalComponent
    {
        friend class JDControlService;
        friend class JACDAC;
        // the above need direct access to our member variables and more

        protected:

        // Due to the dynamic nature of JACDAC when a new service is created, this variable is incremented.
        // JACDAC id's are allocated from 3000 - 4000
        static uint32_t dynamicId;

        JDServiceMode mode;
        uint32_t service_class;
        uint16_t service_number;
        uint8_t service_flags;

        JDDevice* device;
        JDDevice* requiredDevice;

        /**
         * Called by the logic service when a new state is connected to the serial bus
         *
         * @param state an instance of JDServiceState representing the device that has been connected
         *
         * @return SERVICE_STATE_OK for success
         **/
        virtual int hostConnected();

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
        virtual int send(uint8_t* buf, int len);

        public:

        /**
         * Constructor
         *
         * */
        JDService(uint32_t serviceClass, JDServiceMode m);

        /**
         * Invoked by the logic service when it is queuing a control packet.
         *
         * This allows the addition of service specific control packet information and the setting of any additional flags.
         *
         * @param p A pointer to the packet, where the data field contains a pre-filled control packet.
         *
         * @return SERVICE_STATE_OK on success
         **/
        virtual int addAdvertisementData(uint8_t* data);

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
        virtual int handleServiceInformation(JDDevice* device, JDServiceInformation* info);

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
         * Retrieves the device instance of the remote device
         *
         * @return the address.
         **/
        JDDevice* getHostDevice();

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
}

#endif
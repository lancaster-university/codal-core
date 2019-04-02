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

#ifndef CODAL_JACDAC_H
#define CODAL_JACDAC_H

#include "CodalConfig.h"
#include "CodalComponent.h"
#include "ErrorNo.h"
#include "Event.h"
#include "JDControlService.h"
#include "ManagedString.h"
#include "codal_target_hal.h"

#define JD_STARTED                                      0x02

#ifndef JD_SERVICE_ARRAY_SIZE
#define JD_SERVICE_ARRAY_SIZE                           20
#endif

namespace codal
{
    /**
     * This class is the composition of the physical and control layers. It process packets
     * produced by the JACDAC physical layer and passes them to our high level services using the
     * control layer.
     *
     * This composition forms JACDAC.
     **/
    class JACDAC : public CodalComponent
    {
        friend class JDControlService;
        friend class JDPhysicalLayer;

        /**
         * Invoked by JACDAC when a packet is received from the serial bus.
         *
         * This handler is invoked in standard conext. This also means that users can stop the device from handling packets.
         * (which might be a bad thing).
         *
         * This handler continues to pull packets from the queue and iterate over services.
         **/
        void onPacketReceived(Event);

        // An instance of the control service responsible for routing packets to services
        JDControlService controlService;

        // A pointer to a bridge service (if set). Defaults to NULL.
        JDService* bridge;

        public:

        // this array holds all services on the device
        static JDService* services[JD_SERVICE_ARRAY_SIZE];

        // a reference to a the physical bus instance
        JDPhysicalLayer& bus;

        // a singleton pointer to the current instance of JACDAC.
        static JACDAC* instance;

        /**
         * Constructor
         *
         * @param bus A reference to an instance of JDPhysicalLayer for the transmission of packets.
         *
         * @param deviceName a ManagedString containing the desired name of the device.
         *                   This can be set using setDeviceName post construction.
         *π
         * @param id for the message bus, defaults to  SERVICE_STATE_ID_JACDAC_PROTOCOL
         **/
        JACDAC(JDPhysicalLayer& bus, ManagedString deviceName = ManagedString(), uint16_t id = DEVICE_ID_JACDAC);

        /**
         * Sets the bridge member variable to the given JDService pointer.
         *
         * Bridge services are given all packets received on the bus, the idea being that
         * packets can be bridged to another networking medium, i.e. packet radio.
         *
         * @param bridge the service to forward all packets to another networking medium
         *        this service will receive all packets via the handlePacket call. If NULL
         *        is given, the bridge member variable is cleared.
         **/
        int setBridge(JDService* bridge);

        /**
         * Set the name to be used when enumerated on the bus.
         *
         * @param s the name to use
         **/
        static int setDeviceName(ManagedString s);

        /**
         * Get the name to used when enumerated on the bus.
         *
         * @return the current name being used
         **/
        static ManagedString getDeviceName();

        /**
         * Adds a service to the services array. The control service iterates over this array.
         *
         * @param device a reference to the service to add.
         *
         * @return DEVICE_OK on success.
         **/
        virtual int add(JDService& device);

        /**
         * removes a service from the services array. The control service iterates over this array.
         *
         * @param device a reference to the service to remove.
         *
         * @return DEVICE_OK on success.
         **/
        virtual int remove(JDService& device);

        /**
         * A static method to send an entire, premade JDPacket on the bus.
         *
         * @param pkt the JDPacket struct to send.
         *
         * @return DEVICE_OK on success.
         *
         * @note This struct is copied into memory managed by the physical layer. Users
         *       are responsible for freeing memory allocated to the given JDPacket (pkt).
         **/
        static int send(JDPacket* pkt);

        static int triggerRemoteIdentification(uint8_t device_address);

        static int setRemoteDeviceName(uint8_t device_address, ManagedString name);

        /**
         * Logs the current state of JACDAC services.
         **/
        void logState();

        /**
         * Starts the jacdac bus and enumerates this device (if required).
         *
         * @return DEVICE_OK on success.
         **/
        int start();

        /**
         * Stops the jacdac bus and stops enumeration by the control service (if required).
         *
         * @return DEVICE_OK on success.
         **/
        int stop();
    };

} // namespace codal

#endif

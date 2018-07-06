#ifndef CODAL_MESSAGE_BUS_DRIVER_H
#define CODAL_MESSAGE_BUS_DRIVER_H

#include "PktSerialProtocol.h"
#include "MessageBus.h"

#define PKT_MESSAGEBUS_TYPE_EVENT       0x01
#define PKT_MESSAGEBUS_TYPE_LISTEN      0x02

namespace codal
{
    class PktMessageBusDriver : public PktSerialDriver
    {
        void eventReceived(Event e);
        bool suppressForwarding;

        public:
        PktMessageBusDriver(PktSerialProtocol& proto, bool remote, uint32_t serial = 0);

        /**
         * Associates the given event with the serial channel.
         *
         * Once registered, all events matching the given registration sent to this micro:bit's
         * default EventModel will be automatically retransmitted on the serial bus.
         *
         * @param id The id of the event to register.
         *
         * @param value the value of the event to register.
         *
         * @return DEVICE_OK on success, or DEVICE_NO_RESOURCES if no default EventModel is available.
         *
         * @note The wildcards DEVICE_ID_ANY and DEVICE_EVT_ANY can also be in place of the
         *       id and value fields.
         */
        int listen(uint16_t id, uint16_t value);

        /**
         * Associates the given event with the serial channel.
         *
         * Once registered, all events matching the given registration sent to the given
         * EventModel will be automatically retransmitted on the serial bus.
         *
         * @param id The id of the events to register.
         *
         * @param value the value of the event to register.
         *
         * @param eventBus The EventModel to listen for events on.
         *
         * @return DEVICE_OK on success.
         *
         * @note The wildcards DEVICE_ID_ANY and DEVICE_EVT_ANY can also be in place of the
         *       id and value fields.
         */
        int listen(uint16_t id, uint16_t value, EventModel &eventBus);

        /**
         * Disassociates the given event with the serial channel.
         *
         * @param id The id of the events to deregister.
         *
         * @param value The value of the event to deregister.
         *
         * @return DEVICE_OK on success, or DEVICE_INVALID_PARAMETER if the default message bus does not exist.
         *
         * @note DEVICE_EVT_ANY can be used to deregister all event values matching the given id.
         */
        int ignore(uint16_t id, uint16_t value);
        /**
         * Disassociates the given events with the serial channel.
         *
         * @param id The id of the events to deregister.
         *
         * @param value The value of the event to deregister.
         *
         * @param eventBus The EventModel to deregister on.
         *
         * @return DEVICE_OK on success.
         *
         * @note DEVICE_EVT_ANY can be used to deregister all event values matching the given id.
         */
        int ignore(uint16_t id, uint16_t value, EventModel &eventBus);

        virtual void handleControlPacket(ControlPacket* cp);

        virtual void handlePacket(PktSerialPkt* p);
    };
}

#endif
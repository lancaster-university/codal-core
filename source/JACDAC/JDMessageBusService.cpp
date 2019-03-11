#include "JDMessageBusDriver.h"
#include "CodalDmesg.h"

using namespace codal;

JDMessageBusDriver::JDMessageBusDriver() :
    JDDriver(JDDevice(BroadcastDriver, JD_DRIVER_CLASS_MESSAGE_BUS))
{
    suppressForwarding = false;
}

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
int JDMessageBusDriver::listen(uint16_t id, uint16_t value)
{
    if (EventModel::defaultEventBus)
        return listen(id, value, *EventModel::defaultEventBus);

    return DEVICE_NO_RESOURCES;
}

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
int JDMessageBusDriver::listen(uint16_t id, uint16_t value, EventModel &eventBus)
{
    return eventBus.listen(id, value, this, &JDMessageBusDriver::eventReceived, MESSAGE_BUS_LISTENER_IMMEDIATE);
}

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
int JDMessageBusDriver::ignore(uint16_t id, uint16_t value)
{
    if (EventModel::defaultEventBus)
        return ignore(id, value, *EventModel::defaultEventBus);

    return DEVICE_INVALID_PARAMETER;
}

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
int JDMessageBusDriver::ignore(uint16_t id, uint16_t value, EventModel &eventBus)
{
    return eventBus.ignore(id, value, this, &JDMessageBusDriver::eventReceived);
}


/**
  * Protocol handler callback. This is called when the serial bus receives a packet marked as using the event protocol.
  *
  * This function process this packet, and fires the event contained inside onto the default EventModel.
  */
int JDMessageBusDriver::handlePacket(JDPacket* p)
{
    Event *e = (Event *) p->data;

    suppressForwarding = true;
    e->fire();
    suppressForwarding = false;

    return DEVICE_OK;
}

int JDMessageBusDriver::handleControlPacket(JDControlPacket*)
{
    return DEVICE_OK;
}

/**
  * Event handler callback. This is called whenever an event is received matching one of those registered through
  * the registerEvent() method described above. Upon receiving such an event, it is wrapped into
  * a serial bus packet and transmitted to any other micro:bits in the same group.
  */
void JDMessageBusDriver::eventReceived(Event e)
{
    // DMESG("EVENT");
    if(suppressForwarding)
        return;

    // DMESG("PACKET QUEUED: %d %d %d", e.source, e.value, sizeof(Event));
    send((uint8_t *)&e, sizeof(Event));
    // DMESG("RET %d",ret);
}
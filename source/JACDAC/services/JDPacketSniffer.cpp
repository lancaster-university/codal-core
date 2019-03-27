#include "JDPacketSniffer.h"
#include "JDControlService.h"
#include "CodalDmesg.h"
#include "Timer.h"

using namespace codal;

void JDPacketSniffer::timerCallback(Event)
{
    JDDevice* head = this->deviceManager.getDevice();

    while (head)
    {
        JDDevice* dev = head;
        head = head->next;
        dev->rolling_counter++;

        if (dev->rolling_counter > 3)
        {
            this->deviceManager.removeDevice(dev);
            free(dev->name);
            free(dev);
        }
    }
}

JDPacketSniffer::JDPacketSniffer() : JDService(JD_SERVICE_CLASS_BRIDGE, ClientService)
{
    if (EventModel::defaultEventBus)
    {
        EventModel::defaultEventBus->listen(this->id, JD_CONTROL_SERVICE_EVT_TIMER_CALLBACK, this, &JDPacketSniffer::timerCallback);
        system_timer_event_every(500, this->id, JD_CONTROL_SERVICE_EVT_TIMER_CALLBACK);
    }

}

int JDPacketSniffer::handlePacket(JDPacket* p)
{
    if (p->device_address == 0)
    {
        if (p->service_number == 0)
        {
            JDControlPacket* cp = (JDControlPacket *)p->data;
            if (cp->device_flags & (JD_DEVICE_FLAGS_REJECT | JD_DEVICE_FLAGS_PROPOSING))
                return DEVICE_OK;

            JDDevice* remote = this->deviceManager.addDevice((JDControlPacket*)p->data, p->communication_rate);
            remote->rolling_counter = 0;
        }
    }

    return DEVICE_OK;
}

void JDPacketSniffer::logDevices()
{
    JDDevice* head = this->deviceManager.getDevice();

    while (head)
    {
        DMESG("A: %d, udidL: %d N: %s CR: %d", head->device_address, (uint32_t)head->udid, head->name ? head->name : 0, head->communication_rate);
        head = head->next;
    }
}
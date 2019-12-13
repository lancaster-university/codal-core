#include "JDPacketSniffer.h"
#include "JDControlService.h"
#include "CodalDmesg.h"
#include "Timer.h"

using namespace codal;

void JDPacketSniffer::timerCallback(Event)
{
    JDDevice* head = this->deviceManager.getDeviceList();

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

JDPacketSniffer::JDPacketSniffer() : JDService(JD_SERVICE_IDENTIFIER_BRIDGE, ClientService)
{
    if (EventModel::defaultEventBus)
    {
        EventModel::defaultEventBus->listen(this->id, JD_CONTROL_SERVICE_EVT_TIMER_CALLBACK, this, &JDPacketSniffer::timerCallback);
        system_timer_event_every(500, this->id, JD_CONTROL_SERVICE_EVT_TIMER_CALLBACK);
    }

}

JDDevice* JDPacketSniffer::getDeviceList()
{
    return this->deviceManager.getDeviceList();
}

int JDPacketSniffer::handlePacket(JDPacket* p)
{
    if (p->service_number == 0)
    {
        JDControlPacket* cp = (JDControlPacket *)p->data;
        if (cp->device_flags & (JD_DEVICE_FLAGS_REJECT | JD_DEVICE_FLAGS_PROPOSING))
            return DEVICE_OK;

        JDDevice* remote = this->deviceManager.addDevice(p->device_identifier, (JDControlPacket*)p->data);
        remote->rolling_counter = 0;
    }

    return DEVICE_OK;
}

void JDPacketSniffer::logDevices()
{
    JDDevice* head = this->deviceManager.getDeviceList();

    while (head)
    {
        DMESG("A: %d, device_identifierL: %d N: %s CR: %d", head->device_identifier, (uint32_t)head->device_identifier, head->name ? head->name : 0);
        head = head->next;
    }
}
#include "JDAccelerometerService.h"
#include "CodalDmesg.h"
#include "Timer.h"
#include "EventModel.h"

using namespace codal;

void JDAccelerometerService::sendData(Event)
{
    this->latest = this->accelerometer->getSample();

    AccelerometerPacket p;
    // raw accel type
    p.packet_type = 0;
    p.x = latest.x;
    p.y = latest.y;
    p.z = latest.z;

    send((uint8_t*)&p, sizeof(AccelerometerPacket));
}

void JDAccelerometerService::forwardEvent(Event evt)
{
    if (evt.value == ACCELEROMETER_EVT_3G || evt.value == ACCELEROMETER_EVT_6G || evt.value ==  ACCELEROMETER_EVT_8G || evt.value ==  ACCELEROMETER_EVT_DATA_UPDATE)
        return;

    AccelerometerGesturePacket p;
    // gesture type
    p.packet_type = 1;
    p.event_value = evt.value;

    send((uint8_t*)&p, sizeof(AccelerometerGesturePacket));
}

JDAccelerometerService::JDAccelerometerService(Accelerometer& accel) : JDService(JDServiceState(HostService, JD_DRIVER_CLASS_ACCELEROMETER)), accelerometer(&accel)
{
    system_timer_event_every(50, this->id, JD_ACCEL_EVT_SEND_DATA);

    if (EventModel::defaultEventBus)
    {
        EventModel::defaultEventBus->listen(this->id, JD_ACCEL_EVT_SEND_DATA, this, &JDAccelerometerService::sendData);
        EventModel::defaultEventBus->listen(DEVICE_ID_GESTURE, DEVICE_EVT_ANY, this, &JDAccelerometerService::forwardEvent);
    }
}

JDAccelerometerService::JDAccelerometerService() : JDService(JDServiceState(ClientService, JD_DRIVER_CLASS_ACCELEROMETER)), accelerometer(NULL)
{
}

int JDAccelerometerService::getX()
{
    return latest.x;
}
int JDAccelerometerService::getY()
{
    return latest.y;
}
int JDAccelerometerService::getZ()
{
    return latest.z;
}

int JDAccelerometerService::handleControlPacket(JDControlPacket* cp)
{
    return DEVICE_OK;
}

int JDAccelerometerService::handlePacket(JDPacket* p)
{
    AccelerometerPacket* data = (AccelerometerPacket*)p->data;

    if (data->packet_type == 0)
    {
        latest.x = data->x;
        latest.y = data->y;
        latest.z = data->z;
    }

    if (data->packet_type == 1)
    {
        AccelerometerGesturePacket* gesture =  (AccelerometerGesturePacket*)p->data;
        Event(this->id, gesture->event_value);
    }


    return DEVICE_OK;
}
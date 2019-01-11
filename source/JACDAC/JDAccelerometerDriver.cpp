#include "JDAccelerometerDriver.h"
#include "CodalDmesg.h"
#include "Timer.h"
#include "EventModel.h"

using namespace codal;

void JDAccelerometerDriver::sendData(Event)
{
    this->latest = this->accelerometer->getSample();

    AccelerometerPacket p;
    // raw accel type
    p.packet_type = 0;
    p.x = latest.x;
    p.y = latest.y;
    p.z = latest.z;

    JDProtocol::send((uint8_t*)&p, sizeof(AccelerometerPacket), this->device.address);
}

void JDAccelerometerDriver::forwardEvent(Event evt)
{
    if (evt.value == ACCELEROMETER_EVT_3G || evt.value == ACCELEROMETER_EVT_6G || evt.value ==  ACCELEROMETER_EVT_8G || evt.value ==  ACCELEROMETER_EVT_DATA_UPDATE)
        return;

    AccelerometerGesturePacket p;
    // gesture type
    p.packet_type = 1;
    p.event_value = evt.value;

    JDProtocol::send((uint8_t*)&p, sizeof(AccelerometerGesturePacket), this->device.address);
}

JDAccelerometerDriver::JDAccelerometerDriver(Accelerometer& accel) : JDDriver(JDDevice(HostDriver, JD_DRIVER_CLASS_ACCELEROMETER)), accelerometer(&accel)
{
    system_timer_event_every(50, this->id, JD_ACCEL_EVT_SEND_DATA);

    if (EventModel::defaultEventBus)
    {
        EventModel::defaultEventBus->listen(this->id, JD_ACCEL_EVT_SEND_DATA, this, &JDAccelerometerDriver::sendData);
        EventModel::defaultEventBus->listen(DEVICE_ID_GESTURE, DEVICE_EVT_ANY, this, &JDAccelerometerDriver::forwardEvent);
    }
}

JDAccelerometerDriver::JDAccelerometerDriver() : JDDriver(JDDevice(VirtualDriver, JD_DRIVER_CLASS_ACCELEROMETER)), accelerometer(NULL)
{
}

int JDAccelerometerDriver::getX()
{
    return latest.x;
}
int JDAccelerometerDriver::getY()
{
    return latest.y;
}
int JDAccelerometerDriver::getZ()
{
    return latest.z;
}

int JDAccelerometerDriver::handleControlPacket(JDPacket* cp)
{
    return DEVICE_OK;
}

int JDAccelerometerDriver::handlePacket(JDPacket* p)
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
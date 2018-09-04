#include "JDBroadcastDummy.h"

using namespace codal;

JDBroadcastDummy::JDBroadcastDummy(JDDevice d) :
    JDDriver(d, DEVICE_ID_JD_BROADCAST_DRIVER)
{
    if (JDProtocol::instance)
        JDProtocol::instance->add(*this);
    this->device.flags |= JD_DEVICE_FLAGS_BROADCAST_MAP;
}

int JDBroadcastDummy::deviceRemoved()
{
    // a bit risky, but this will also delete us from the array of drivers
    delete this;
    return DEVICE_OK;
}

int JDBroadcastDummy::handleControlPacket(JDPkt*)
{
    // we don't handle any packets in this driver
    return DEVICE_CANCELLED;
}

int JDBroadcastDummy::handlePacket(JDPkt*)
{
    // we don't handle any packets in this driver
    return DEVICE_CANCELLED;
}
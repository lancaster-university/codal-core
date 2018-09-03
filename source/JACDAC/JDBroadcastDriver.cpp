#include "JDBroadcastDriver.h"

using namespace codal;

JDBroadcastDriver::JDBroadcastDriver(JDDevice d) :
    JDDriver(d, DEVICE_ID_JD_BROADCAST_DRIVER)
{
    if (JDProtocol::instance)
        JDProtocol::instance->add(*this);
    this->device.flags |= JD_DEVICE_FLAGS_BROADCAST_MAP;
}

int JDBroadcastDriver::deviceRemoved()
{
    // a bit risky, but this will also delete us from the array of drivers
    delete this;
    return DEVICE_OK;
}

int JDBroadcastDriver::handleControlPacket(JDPkt*)
{
    // we don't handle any packets in this driver
    return DEVICE_CANCELLED;
}

int JDBroadcastDriver::handlePacket(JDPkt*)
{
    // we don't handle any packets in this driver
    return DEVICE_CANCELLED;
}
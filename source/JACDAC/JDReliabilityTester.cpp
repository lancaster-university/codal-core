#include "JDReliabilityTester.h"
#include "CodalDmesg.h"
#include "CodalFiber.h"

using namespace codal;

JDReliabilityTester::JDReliabilityTester(Pin& p, uint32_t max_count) : JDDriver(JDDevice(HostDriver, JD_DRIVER_CLASS_RELIABILITY_TESTER), dynamicId++), pin(&p)
{
    this->max_count = max_count;
    this->received = (uint8_t*)malloc(this->max_count);
    memset(received, 0, this->max_count);
}

JDReliabilityTester::JDReliabilityTester() :  JDDriver(JDDevice(VirtualDriver, JD_DRIVER_CLASS_RELIABILITY_TESTER), dynamicId++), pin(NULL), received(NULL)
{
}

int JDReliabilityTester::sendPacket(uint8_t value)
{
    if (!(this->device.flags & JD_DEVICE_FLAGS_REMOTE) || !this->isConnected())
        return DEVICE_INVALID_STATE;

    ReliabilityPacket p;

    p.value = value;
    p.count = count++;

    return JDProtocol::send((uint8_t*)&p, sizeof(ReliabilityPacket), this->device.address);
}

int JDReliabilityTester::start()
{
    if (this->device.flags & JD_DEVICE_FLAGS_REMOTE && this->status & RELIABILITY_STATUS_TEST_READY)
    {
        this->count = 0;
        int state = 0;

        for (uint32_t i = 0; i < this->max_count; i++)
        {
            sendPacket(state);
            state = !state;
            fiber_sleep(100);
        }
    }
    else
    {
        this->status = RELIABILITY_STATUS_TEST_READY;
        fiber_wait_for_event(this->id, RELIABILITY_TEST_FINISHED);

        int rx_count = 0;

        DMESG("Missed: ");

        for (uint32_t i = 0; i < this->max_count; i++)
        {
            if (received[i])
                rx_count++;
            else
                DMESG("%d ", i);
        }

        DMESG("Reliability: %d", (int)(((float)rx_count) / (float)this->max_count * 100.0));
    }

    return DEVICE_OK;
}

int JDReliabilityTester::fillControlPacket(JDPkt* p)
{
    ControlPacket* cp = (ControlPacket*)p->data;
    ReliabilityAdvertisement* ra = (ReliabilityAdvertisement*)cp->data;
    ra->status = this->status;
    ra->max_count = this->max_count;
    return DEVICE_OK;
}

int JDReliabilityTester::handleControlPacket(JDPkt* p)
{
    ControlPacket* cp = (ControlPacket*)p->data;

    if (this->device.flags & JD_DEVICE_FLAGS_REMOTE)
    {
        ReliabilityAdvertisement* ra = (ReliabilityAdvertisement*)cp->data;

        this->max_count = ra->max_count;

        if (ra->status & RELIABILITY_STATUS_TEST_READY)
        {
            this->status |= RELIABILITY_STATUS_TEST_READY;
        }
    }

    return DEVICE_OK;
}

int JDReliabilityTester::handlePacket(JDPkt* p)
{
    ReliabilityPacket* pinData = (ReliabilityPacket*)p->data;

    // if remote we ignore received packets.
    if ((this->device.flags & JD_DEVICE_FLAGS_REMOTE))
        return DEVICE_CANCELLED;

    // if we're paired and a random device has sent us a packet ignore
    if (isPaired() && this->pairedInstance->getAddress() != p->address )
        return DEVICE_OK;


    if (pinData->count == 0)
    {
        this->count = 0;
        this->status = RELIABILITY_STATUS_TEST_IN_PROGRESS;
    }

    if (pinData->count == this->max_count - 1)
        Event(this->id, RELIABILITY_TEST_FINISHED);

    if (pinData->count == this->count)
    {
        received[pinData->count] = 1;
        this->count++;
    }
    else
        this->count = pinData->count;

    pin->setDigitalValue(pinData->value);

    return DEVICE_OK;
}
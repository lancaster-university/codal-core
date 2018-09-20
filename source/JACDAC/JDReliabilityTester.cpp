#include "JDReliabilityTester.h"
#include "CodalDmesg.h"
#include "CodalFiber.h"

using namespace codal;

static uint8_t received[RELIABILITY_TEST_MAX_COUNT] = { 0 };

JDReliabilityTester::JDReliabilityTester(Pin& p, uint32_t max_count) : JDDriver(JDDevice(HostDriver, JD_DRIVER_CLASS_RELIABILITY_TESTER), dynamicId++), pin(&p)
{
    this->max_count = max_count;
    memset(received, 0, RELIABILITY_TEST_MAX_COUNT);
}

JDReliabilityTester::JDReliabilityTester() : JDDriver(JDDevice(VirtualDriver, JD_DRIVER_CLASS_RELIABILITY_TESTER), dynamicId++), pin(NULL)
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

        for (int i = 0; i < RELIABILITY_TEST_MAX_COUNT; i++)
        {
            sendPacket(state);
            state = !state;
            fiber_sleep(100);
        }
    }
    else
    {
        fiber_wait_for_event(this->id, RELIABILITY_TEST_FINISHED);

        int rx_count = 0;

        DMESG("Missed: ");

        for (int i = 0; i < RELIABILITY_TEST_MAX_COUNT; i++)
        {
            if (received[i])
                rx_count++;
            else
                DMESG("%d ", i);
        }

        DMESG("Reliability: %d", (int)(((float)rx_count) / (float)RELIABILITY_TEST_MAX_COUNT * 100.0));
    }

    return DEVICE_OK;
}

int JDReliabilityTester::fillControlPacket(JDPkt* p)
{
    ControlPacket* cp = (ControlPacket*)p->data;
    ReliabilityAdvertisement* ra = (ReliabilityAdvertisement*)cp->data;
    ra->status = this->status;
    ra->max_count = this->max_count;
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

    if (pinData->count == RELIABILITY_TEST_MAX_COUNT - 1)
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
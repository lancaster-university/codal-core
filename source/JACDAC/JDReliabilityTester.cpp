#include "JDReliabilityTester.h"
#include "CodalDmesg.h"
#include "CodalFiber.h"

using namespace codal;

JDReliabilityTester::JDReliabilityTester(Pin& p, uint32_t max_count) : JDService(JD_SERVICE_CLASS_RELIABILITY_TESTER, ClientService), pin(&p)
{
    this->max_count = max_count;
    this->received = (uint8_t*)malloc(this->max_count);
    memset(received, 0, this->max_count);
}

JDReliabilityTester::JDReliabilityTester() :  JDService(JD_SERVICE_CLASS_RELIABILITY_TESTER, HostService), pin(NULL), received(NULL)
{
}

int JDReliabilityTester::sendPacket(uint8_t value)
{
    if (!(this->mode == ClientService) || !this->isConnected())
        return DEVICE_INVALID_STATE;

    ReliabilityPacket p;

    p.value = value;
    p.count = count++;

    return send((uint8_t*)&p, sizeof(ReliabilityPacket));
}

int JDReliabilityTester::start()
{
    if (this->mode == ClientService && this->status & RELIABILITY_STATUS_TEST_READY)
    {
        this->count = 0;
        int state = 0;

        for (uint32_t i = 0; i < this->max_count; i++)
        {
            sendPacket(state);
            state = !state;
            if (i == this->max_count - 1)
                fiber_wait_for_event(this->id, RELIABILITY_TEST_FINISHED);
            else
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
        this->max_count = (uint32_t)(((float)rx_count) / (float)this->max_count * 100.0);
        this->status = RELIABILITY_STATUS_TEST_FINISHED;
    }

    DMESG("Reliability: %d", this->max_count);

    return this->max_count;
}

int JDReliabilityTester::addAdvertisementData(uint8_t* data)
{
    ReliabilityAdvertisement* ra = (ReliabilityAdvertisement*)data;
    ra->status = this->status;
    ra->max_count = this->max_count;
    return sizeof(ReliabilityAdvertisement);
}

int JDReliabilityTester::handleServiceInformation(JDDevice* device, JDServiceInformation* p)
{
    JDControlPacket* cp = (JDControlPacket*)p->data;

    if (this->mode == ClientService)
    {
        ReliabilityAdvertisement* ra = (ReliabilityAdvertisement*)cp->data;

        if (ra->status & RELIABILITY_STATUS_TEST_READY)
        {
            this->max_count = ra->max_count;
            this->status |= RELIABILITY_STATUS_TEST_READY;
            // DMESG("READY: %d",this->max_count);
        }

        if (ra->status & RELIABILITY_STATUS_TEST_FINISHED)
        {
            this->status &= ~RELIABILITY_STATUS_TEST_READY;
            this->max_count = ra->max_count;
            Event(this->id, RELIABILITY_TEST_FINISHED);
        }
    }

    return DEVICE_OK;
}

int JDReliabilityTester::handlePacket(JDPacket* p)
{
    ReliabilityPacket* pinData = (ReliabilityPacket*)p->data;

    // if client we ignore received packets.
    if ((this->mode == ClientService))
        return DEVICE_CANCELLED;

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
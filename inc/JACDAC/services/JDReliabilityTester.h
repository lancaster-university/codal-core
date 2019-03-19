#ifndef JD_RELIABILITY_TESTER_H
#define JD_RELIABILITY_TESTER_H

#include "JACDAC.h"
#include "Pin.h"

#define RELIABILITY_TEST_FINISHED 7

#define RELIABILITY_STATUS_TEST_IN_PROGRESS     0x02
#define RELIABILITY_STATUS_TEST_READY           0x04
#define RELIABILITY_STATUS_TEST_FINISHED        0x08

namespace codal
{
    struct ReliabilityAdvertisement
    {
        uint8_t status;
        uint32_t max_count;
    };

    struct ReliabilityPacket
    {
        uint8_t value;
        uint32_t count;
    };

    class JDReliabilityTester : public JDService
    {
        Pin* pin;

        int sendPacket(uint8_t value);

        uint32_t count;
        uint32_t max_count;

        uint8_t* received;

        public:
        JDReliabilityTester(Pin& p, uint32_t max = 1000);

        JDReliabilityTester();

        int start();

        virtual int addAdvertisementData(uint8_t* data) override;

        virtual int handleServiceInformation(JDDevice* device, JDServiceInformation* info) override;

        virtual int handlePacket(JDPacket* p) override;
    };
}

#endif
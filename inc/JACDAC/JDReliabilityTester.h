#ifndef JD_RELIABILITY_TESTER_H
#define JD_RELIABILITY_TESTER_H

#include "JDProtocol.h"
#include "Pin.h"

#define RELIABILITY_TEST_FINISHED 2

#define RELIABILITY_STATUS_TEST_IN_PROGRESS    0x02
#define RELIABILITY_STATUS_TEST_READY          0x04

#define RELIABILITY_TEST_MAX_COUNT      1000

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

    class JDReliabilityTester : public JDDriver
    {
        Pin* pin;

        int sendPacket(uint8_t value);

        uint32_t count;
        uint32_t max_count;

        public:
        JDReliabilityTester(Pin& p, uint32_t max = 1000);

        JDReliabilityTester();

        int start();

        virtual int fillControlPacket(JDPkt* cp);

        virtual int handleControlPacket(JDPkt* cp);

        virtual int handlePacket(JDPkt* p);
    };
}

#endif
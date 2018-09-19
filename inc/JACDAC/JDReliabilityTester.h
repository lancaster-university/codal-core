#ifndef JD_RELIABILITY_TESTER_H
#define JD_RELIABILITY_TESTER_H

#include "JDProtocol.h"
#include "Pin.h"

#define RELIABILITY_TEST_FINISHED 2

#define RELIABILITY_TEST_MAX_COUNT      1000

namespace codal
{
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
        uint32_t outOfSequenceCount;

        public:
        JDReliabilityTester(Pin& p);

        JDReliabilityTester();

        int start();

        virtual int handleControlPacket(JDPkt* cp);

        virtual int handlePacket(JDPkt* p);
    };
}

#endif
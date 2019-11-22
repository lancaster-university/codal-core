#include "JDService.h"
#include "ManagedString.h"

#ifndef JD_TEST_SERVICE_H
#define JD_TEST_SERVICE_H

namespace codal
{
    class JDTestService : public JDService
    {
        ManagedString name;

        public:
        JDTestService(ManagedString serviceName, JDServiceMode m);

        virtual int handlePacket(JDPacket* p) override;

        void sendTestPacket(uint32_t value);
    };
}

#endif
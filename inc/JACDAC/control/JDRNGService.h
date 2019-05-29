#include "JDService.h"

#ifndef JD_RNG_SERVICE_H
#define JD_RNG_SERVICE_H

#define JD_CONTROL_RNG_SERVICE_NUMBER                   2

#define JD_CONTROL_RNG_SERVICE_REQUEST_TYPE_REQ         1
#define JD_CONTROL_RNG_SERVICE_REQUEST_TYPE_RESP        2

struct JDRNGServicePacket
{
    uint32_t request_type;
    uint32_t random;
};

namespace codal
{
    class JDRNGService : public JDService
    {
        int send(uint8_t* buf, int len) override;

        public:
        JDRNGService();

        virtual int handlePacket(JDPacket* p) override;
    };
}

#endif
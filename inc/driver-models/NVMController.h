#ifndef CODAL_NVM_CTRL_H
#define CODAL_NVM_CTRL_H

#include "CodalConfig.h"

namespace codal
{
    class NVMController
    {
        public:

        virtual uint32_t* getFlashEnd() = 0;

        virtual uint32_t* getFlashStart() = 0;

        virtual uint32_t getPageSize() = 0;

        virtual uint32_t getFlashSize() = 0;

        virtual int copy(uint32_t* dest, uint32_t* source, uint32_t size) = 0;

        virtual int erase(uint32_t* page) = 0;

        virtual int write(uint32_t* dst, uint32_t* source, uint32_t size) = 0;
    };
}

#endif
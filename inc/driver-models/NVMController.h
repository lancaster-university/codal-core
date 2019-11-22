#ifndef CODAL_NVM_CTRL_H
#define CODAL_NVM_CTRL_H

#include "CodalConfig.h"
#include "ErrorNo.h"

namespace codal
{
    class NVMController
    {
        public:

        NVMController(){};

        virtual uint32_t* getFlashEnd()
        {
            return NULL;
        }

        virtual uint32_t* getFlashStart()
        {
            return NULL;
        }

        virtual uint32_t getPageSize()
        {
            return 0;
        }

        virtual uint32_t getFlashSize()
        {
            return 0;
        }

        virtual int copy(uint32_t* dest, uint32_t* source, uint32_t size)
        {
            return DEVICE_NOT_IMPLEMENTED;
        }

        virtual int erase(uint32_t* page)
        {
            return DEVICE_NOT_IMPLEMENTED;
        }

        virtual int write(uint32_t* dst, uint32_t* source, uint32_t size)
        {
            return DEVICE_NOT_IMPLEMENTED;
        }
    };
}

#endif
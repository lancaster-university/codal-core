#ifndef DMA_SINGLE_WIRE_SERIAL_H
#define DMA_SINGLE_WIRE_SERIAL_H

#include "SingleWireSerial.h"
#include "DMASingleWireSerial.h"
#include "MemberFunctionCallback.h"

namespace codal
{
    class DMASingleWireSerial : public SingleWireSerial
    {

        public:

        MemberFunctionCallback* cb;

        DMASingleWireSerial(Pin& p) : SingleWireSerial(p)
        {
            cb = NULL;
        }

        virtual int sendDMA(uint8_t* data, int len) = 0;
        virtual int receiveDMA(uint8_t* data, int len) = 0;

        template <typename T>
        int setDMACompletionHandler(T* object, void (T::*method)(Event e))
        {
            if (!object)
                return DEVICE_INVALID_PARAMETER;

            this->cb = new MemberFunctionCallback(object, method);
            return DEVICE_OK;
        }
    };
}

#endif
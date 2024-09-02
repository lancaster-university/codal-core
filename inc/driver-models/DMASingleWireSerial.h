#ifndef DMA_SINGLE_WIRE_SERIAL_H
#define DMA_SINGLE_WIRE_SERIAL_H

#include "DMASingleWireSerial.h"
#include "MemberFunctionCallback.h"
#include "SingleWireSerial.h"

namespace codal {
class DMASingleWireSerial : public SingleWireSerial {
  public:
    DMASingleWireSerial(Pin& p) : SingleWireSerial(p) { cb = NULL; }

    virtual int sendDMA(uint8_t* data, int len)    = 0;
    virtual int receiveDMA(uint8_t* data, int len) = 0;
    virtual int abortDMA()                         = 0;
};
}  // namespace codal

#endif
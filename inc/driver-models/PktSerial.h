/*
The MIT License (MIT)

Copyright (c) 2017 Lancaster University.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#ifndef CODAL_PKTSERIAL_H
#define CODAL_PKTSERIAL_H

#include "CodalConfig.h"
#include "ErrorNo.h"
#include "Pin.h"

namespace codal
{

#define CODAL_PKTSERIAL_EVT_DATA_RECEIVED 1
#define CODAL_PKTSERIAL_EVT_DATA_SENT 2
#define CODAL_PKTSERIAL_EVT_ERROR 3
#define CODAL_PKTSERIAL_EVT_DATA_DROPPED 4

struct PktSerialPkt {
public:
    PktSerialPkt *next;

    uint16_t size; // not including 'size' field
    uint16_t crc;
    // add more stuff
    uint8_t data[0];

    static PktSerialPkt *alloc(uint16_t sz);
};

/**
 * Class definition for a PktSerial interface.
 */
class PktSerial
{
protected:
    PktSerialPkt *recvQueue;
    void queue(PktSerialPkt *pkt);
    virtual uint32_t getRandom();
public:
    uint16_t id;

    PktSerialPkt *getPacket();

    /**
     * Start to listen.
     */
    virtual void start() = 0;

    /**
     * Disables protocol.
     */
    virtual void stop() = 0;

    /**
     * Writes to the PktSerial bus. Waits (possibly un-scheduled) for transfer to finish.
     */
    virtual int send(const PktSerialPkt *pkt) = 0;
};
} // namespace codal

#endif

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

#include "CodalConfig.h"
#include "PktSerial.h"
#include "ErrorNo.h"
#include "CodalDmesg.h"
#include "MessageBus.h"
#include "Event.h"
#include "CodalFiber.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

namespace codal
{

PktSerialPkt *PktSerialPkt::alloc(uint16_t sz)
{
    auto p = malloc(6 + sz);
    memset(p, 0, 6 + sz);
    return (PktSerialPkt *)p;
}

void PktSerial::queue(PktSerialPkt *pkt)
{
    pkt->next = NULL;

    target_disable_irq();

    if (!recvQueue)
    {
        recvQueue = pkt;
    }
    else
    {
        unsigned i = 0;
        PktSerialPkt *last = recvQueue;
        while (last->next)
        {
            last = last->next;
            i++;
        }

        if (i > 10)
        {
            target_enable_irq();
            Event(id, CODAL_PKTSERIAL_EVT_DATA_DROPPED);
            return;
        }

        last->next = pkt;
    }

    target_enable_irq();
    Event(id, CODAL_PKTSERIAL_EVT_DATA_RECEIVED);
}

PktSerialPkt *PktSerial::getPacket()
{
    PktSerialPkt *r = NULL;

    target_disable_irq();
    if (recvQueue)
    {
        r = recvQueue;
        recvQueue = r->next;
    }
    target_enable_irq();

    return r;
}

uint32_t PktSerial::getRandom()
{
    static int seed = 0;
    seed = 1103515245 * seed + 12345;
    return seed;
}

} // namespace codal

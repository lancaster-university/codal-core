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
    auto p = malloc(2 + sz);
    memset(p, 0, 2 + sz);
    return (PktSerialPkt *)p;
}

void PktSerial::queue(PktSerialPkt *pkt)
{
    unsigned i;
    for (i = 0; i < ARRAY_SIZE(recvQueue); ++i)
    {
        if (recvQueue[i] == NULL)
        {
            recvQueue[i] = pkt;
            Event(id, CODAL_PKTSERIAL_EVT_DATA_RECEIVED);
            return;
        }
    }

#if 0
    free(pkt);
    return;
#else
    free(getPacket());
    recvQueue[i - 1] = pkt;
    Event(id, CODAL_PKTSERIAL_EVT_DATA_RECEIVED);
#endif
}

PktSerialPkt *PktSerial::getPacket()
{
    if (recvQueue[0])
    {
        auto r = recvQueue[0];
        memmove(recvQueue, recvQueue + 1, sizeof(recvQueue) - sizeof(recvQueue[0]));
        recvQueue[ARRAY_SIZE(recvQueue) - 1] = NULL;
        return r;
    }
    return NULL;
}

uint32_t PktSerial::getRandom()
{
    static int seed = 0;
    seed = 1103515245 * seed + 12345;
    return seed;
}

} // namespace codal

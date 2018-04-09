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

#include "SPI.h"
#include "ErrorNo.h"
#include "CodalFiber.h"

namespace codal
{

/**
 * Writes a given command to SPI bus, and afterwards reads the response.
 *
 * Note that bytes recieved while sending command are ignored.
 */
int SPI::transfer(const uint8_t *command, uint32_t commandSize, uint8_t *response,
                  uint32_t responseSize)
{
    for (uint32_t i = 0; i < commandSize; ++i)
    {
        if (write(command[i]) < 0)
            return DEVICE_SPI_ERROR;
    }
    for (uint32_t i = 0; i < responseSize; ++i)
    {
        int c = write(0);
        if (c < 0)
            return DEVICE_SPI_ERROR;
        response[i] = c;
    }
    return DEVICE_OK;
}

/**
 * Writes a given command to SPI bus, and afterwards reads the response. Finally, calls doneHandler
 * (possibly in IRQ context).
 *
 * Note that bytes recieved while sending command are ignored.
 */
int SPI::startTransfer(const uint8_t *command, uint32_t commandSize, uint8_t *response,
                       uint32_t responseSize, void (*doneHandler)(void *), void *arg)
{
    int r = transfer(command, commandSize, response, responseSize);
    // it's important this doesn't get invoked recursievely, since that leads to stack overflow
    create_fiber(doneHandler, arg);
    return r;
}

}

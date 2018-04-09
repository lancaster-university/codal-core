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

#ifndef DEVICE_ST7735_H
#define DEVICE_ST7735_H

#include "Pin.h"
#include "SPI.h"
#include "Event.h"

namespace codal
{

struct ST7735WorkBuffer;

class ST7735
{
    SPI &spi;
    Pin &cs;
    Pin &dc;
    uint8_t cmdBuf[20];
    ST7735WorkBuffer *work;

    void sendCmd(uint8_t *buf, int len);
    void sendCmdSeq(const uint8_t *buf);
    void sendDone(Event);
    void sendWords(unsigned numBytes);
    void startTransfer(unsigned size);
    void sendBytes(unsigned num);

    static void sendColorsStep(ST7735 *st);

public:
    ST7735(SPI &spi, Pin &cs, Pin &dc);
    void init();
    void sendColors(const void *colors, int byteSize);
    void setAddrWindow(int x, int y, int w, int h);
    int sendIndexedImage(const uint8_t *src, unsigned numBytes, uint32_t *palette);
    void waitForSendDone();
};

static inline uint16_t color565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static inline uint16_t color565(uint32_t rgb)
{
    return color565((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
}
}

#endif

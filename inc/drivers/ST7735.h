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
#include "Display.h"

namespace codal
{

struct ST7735WorkBuffer;

#define MADCTL_MY 0x80
#define MADCTL_MX 0x40
#define MADCTL_MV 0x20
#define MADCTL_ML 0x10
#define MADCTL_RGB 0x00
#define MADCTL_BGR 0x08
#define MADCTL_MH 0x04

class ST7735 : public Display
{
    SPI &spi;
    Pin &cs;
    Pin &dc;
    uint8_t cmdBuf[20];
    ST7735WorkBuffer *work;

    uint32_t dimW;
    uint32_t dimH;
    int offX, offY;
    const uint32_t *paletteTable;

    bool autoUpdate;

    void sendCmd(uint8_t *buf, int len);
    void sendCmdSeq(const uint8_t *buf);
    void sendDone(Event);
    void sendWords(unsigned numBytes);
    void startTransfer(unsigned size);
    void sendBytes(unsigned num);
    void startRAMWR(int cmd = 0);

    static void sendColorsStep(ST7735 *st);

    /**
     * Set rectangle where pixels sent by sendIndexedImage() will be stored.
     */
    void setAddrWindow(int x, int y, int w, int h);

    void initDisplay();

    void render(Event);

public:
    ST7735(SPI &spi, Pin &cs, Pin &dc, Pin &reset, Pin &bl, int displayWidth, int displayHeight,
           int noAutoUpdate = 0);

    virtual void enable();

    virtual int setRotation(DisplayRotation r);

    void setPalette(const uint32_t *palette);

    /**
     * Configure screen-specific parameters.
     *
     * @param madctl See MADCTL_* constants above
     * @param frmctr1 defaults to 0x083b3b, 0x053a3a, 0x053c3c depending on screen size; 0x000605
     * was found to work well on 160x128 screen; big-endian
     */
    void configure(uint8_t madctl, uint32_t frmctr1);

    void setOffset(int offX, int offY);

    void beginUpdate();

    /**
     * Waits for the previous beginUpdate() operation to complete (it normally executes in
     * background).
     */
    void waitForEndUpdate();
};

} // namespace codal

#endif

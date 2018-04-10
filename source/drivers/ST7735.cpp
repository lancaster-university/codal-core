#include "ST7735.h"
#include "CodalFiber.h"
#include "CodalDmesg.h"

#define assert(cond)                                                                               \
    if (!(cond))                                                                                   \
    target_panic(909)

#define ST7735_NOP 0x00
#define ST7735_SWRESET 0x01
#define ST7735_RDDID 0x04
#define ST7735_RDDST 0x09

#define ST7735_SLPIN 0x10
#define ST7735_SLPOUT 0x11
#define ST7735_PTLON 0x12
#define ST7735_NORON 0x13

#define ST7735_INVOFF 0x20
#define ST7735_INVON 0x21
#define ST7735_DISPOFF 0x28
#define ST7735_DISPON 0x29
#define ST7735_CASET 0x2A
#define ST7735_RASET 0x2B
#define ST7735_RAMWR 0x2C
#define ST7735_RAMRD 0x2E

#define ST7735_PTLAR 0x30
#define ST7735_COLMOD 0x3A
#define ST7735_MADCTL 0x36

#define ST7735_FRMCTR1 0xB1
#define ST7735_FRMCTR2 0xB2
#define ST7735_FRMCTR3 0xB3
#define ST7735_INVCTR 0xB4
#define ST7735_DISSET5 0xB6

#define ST7735_PWCTR1 0xC0
#define ST7735_PWCTR2 0xC1
#define ST7735_PWCTR3 0xC2
#define ST7735_PWCTR4 0xC3
#define ST7735_PWCTR5 0xC4
#define ST7735_VMCTR1 0xC5

#define ST7735_RDID1 0xDA
#define ST7735_RDID2 0xDB
#define ST7735_RDID3 0xDC
#define ST7735_RDID4 0xDD

#define ST7735_PWCTR6 0xFC

#define ST7735_GMCTRP1 0xE0
#define ST7735_GMCTRN1 0xE1

#define MADCTL_MY 0x80
#define MADCTL_MX 0x40
#define MADCTL_MV 0x20
#define MADCTL_ML 0x10
#define MADCTL_RGB 0x00
#define MADCTL_BGR 0x08
#define MADCTL_MH 0x04

namespace codal
{

ST7735::ST7735(SPI &spi, Pin &cs, Pin &dc) : spi(spi), cs(cs), dc(dc), work(NULL) {}

#define DELAY 0x80

// clang-format off
static const uint8_t initCmds[] = {
    ST7735_SWRESET,   DELAY,  //  1: Software reset, 0 args, w/delay
      120,                    //     150 ms delay
    ST7735_SLPOUT ,   DELAY,  //  2: Out of sleep mode, 0 args, w/delay
      120,                    //     500 ms delay
    ST7735_FRMCTR1, 3      ,  //  3: Frame rate ctrl - normal mode, 3 args:
      0x08, 0x3B, 0x3B,       //     Rate = fosc/(1x2+40) * (LINE+2C+2D)
      #if 0
    ST7735_FRMCTR2, 3      ,  //  4: Frame rate control - idle mode, 3 args:
      0x01, 0x2C, 0x2D,       //     Rate = fosc/(1x2+40) * (LINE+2C+2D)
    ST7735_FRMCTR3, 6      ,  //  5: Frame rate ctrl - partial mode, 6 args:
      0x01, 0x2C, 0x2D,       //     Dot inversion mode
      0x01, 0x2C, 0x2D,       //     Line inversion mode
    ST7735_INVCTR , 1      ,  //  6: Display inversion ctrl, 1 arg, no delay:
      0x07,                   //     No inversion
    ST7735_PWCTR1 , 3      ,  //  7: Power control, 3 args, no delay:
      0xA2,
      0x02,                   //     -4.6V
      0x84,                   //     AUTO mode
    ST7735_PWCTR2 , 1      ,  //  8: Power control, 1 arg, no delay:
      0xC5,                   //     VGH25 = 2.4C VGSEL = -10 VGH = 3 * AVDD
    ST7735_PWCTR3 , 2      ,  //  9: Power control, 2 args, no delay:
      0x0A,                   //     Opamp current small
      0x00,                   //     Boost frequency
    ST7735_PWCTR4 , 2      ,  // 10: Power control, 2 args, no delay:
      0x8A,                   //     BCLK/2, Opamp current small & Medium low
      0x2A,  
    ST7735_PWCTR5 , 2      ,  // 11: Power control, 2 args, no delay:
      0x8A, 0xEE,
    ST7735_VMCTR1 , 1      ,  // 12: Power control, 1 arg, no delay:
      0x0E,
      #endif
    ST7735_INVOFF , 0      ,  // 13: Don't invert display, no args, no delay
    ST7735_COLMOD , 1      ,  // 15: set color mode, 1 arg, no delay:
      0x03,                  //     12-bit color
    ST7735_MADCTL, 1,  MADCTL_MV | MADCTL_MY | MADCTL_BGR,

    ST7735_GMCTRP1, 16      , //  1: Magical unicorn dust, 16 args, no delay:
      0x02, 0x1c, 0x07, 0x12,
      0x37, 0x32, 0x29, 0x2d,
      0x29, 0x25, 0x2B, 0x39,
      0x00, 0x01, 0x03, 0x10,
    ST7735_GMCTRN1, 16      , //  2: Sparkles and rainbows, 16 args, no delay:
      0x03, 0x1d, 0x07, 0x06,
      0x2E, 0x2C, 0x29, 0x2D,
      0x2E, 0x2E, 0x37, 0x3F,
      0x00, 0x00, 0x02, 0x10,
    ST7735_NORON  ,    DELAY, //  3: Normal display on, no args, w/delay
      10,                     //     10 ms delay
    ST7735_DISPON ,    DELAY, //  4: Main screen turn on, no args w/delay
      10,
    0, 0 // END
};
// clang-format on

struct ST7735WorkBuffer
{
    uint32_t *paletteTable;
    uint8_t dataBuf[255];
    bool inProgress;
    const uint8_t *srcPtr;
    unsigned srcLeft;
};

static void processLine(uint8_t *src, uint32_t *dst, int height, int srclen4, int high)
{
    uint32_t s, v;

#define LD()                                                                                       \
    v = 0x111 * (*src >> 4);                                                                       \
    src += height
#define A(sh)                                                                                      \
    LD();                                                                                          \
    s |= v << sh;

    if (high)
        while (srclen4--)
        {
            s = 0;
            A(20);
            A(8);
            LD();
            s |= v >> 4;
            *dst++ = __builtin_bswap32(s);
            s = v << 28;
            A(16);
            A(4);
            LD();
            s |= v >> 8;
            *dst++ = __builtin_bswap32(s);
            s = v << 24;
            A(12);
            A(0);
            *dst++ = __builtin_bswap32(s);
        }
    else
#undef LD
#define LD()                                                                                       \
    v = 0x111 * (*src & 0xf);                                                                      \
    src += height
        while (srclen4--)
        {
            s = 0;
            A(20);
            A(8);
            LD();
            s |= v >> 4;
            *dst++ = __builtin_bswap32(s);
            s = v << 28;
            A(16);
            A(4);
            LD();
            s |= v >> 8;
            *dst++ = __builtin_bswap32(s);
            s = v << 24;
            A(12);
            A(0);
            *dst++ = __builtin_bswap32(s);
        }
}

void ST7735::startTransfer(unsigned size)
{
    spi.startTransfer(work->dataBuf, size, NULL, 0, (PVoidCallback)&ST7735::sendColorsStep, this);
}

void ST7735::sendBytes(unsigned num)
{
    assert(num > 0);
    if (num > work->srcLeft)
        num = work->srcLeft;
    work->srcLeft -= num;
    uint8_t *dst = work->dataBuf;
    while (num--)
    {
        uint32_t v = work->paletteTable[*work->srcPtr++];
        *dst++ = v;
        *dst++ = v >> 8;
        *dst++ = v >> 16;
    }
    startTransfer(dst - work->dataBuf);
}

void ST7735::sendWords(unsigned numBytes)
{
    if (numBytes > work->srcLeft)
        numBytes = work->srcLeft & ~3;
    assert(numBytes > 0);
    work->srcLeft -= numBytes;
    uint32_t numWords = numBytes >> 2;
    const uint32_t *src = (const uint32_t *)work->srcPtr;
    uint32_t *tbl = work->paletteTable;
    uint32_t *dst = (uint32_t *)work->dataBuf;

    while (numWords--)
    {
        uint32_t s = *src++;
        uint32_t o = tbl[s & 0xff];
        uint32_t v = tbl[(s >> 8) & 0xff];
        *dst++ = o | (v << 24);
        o = tbl[(s >> 16) & 0xff];
        *dst++ = (v >> 8) | (o << 16);
        v = tbl[s >> 24];
        *dst++ = (o >> 16) | (v << 8);
    }

    work->srcPtr = (uint8_t *)src;
    startTransfer((uint8_t *)dst - work->dataBuf);
}

void ST7735::sendColorsStep(ST7735 *st)
{
    ST7735WorkBuffer *work = st->work;
    unsigned align = (unsigned)work->srcPtr & 3;
    if (work->srcLeft && align)
    {
        st->sendBytes(4 - align);
    }
    else if (work->srcLeft < 4)
    {
        if (work->srcLeft == 0)
        {
            st->cs.setDigitalValue(1);
            Event(DEVICE_ID_DISPLAY, 100);
        }
        else
        {
            st->sendBytes(work->srcLeft);
        }
    }
    else
    {
        st->sendWords((sizeof(work->dataBuf) / (3 * 4)) * 4);
    }
}

void ST7735::sendDone(Event)
{
    // this executes outside of interrupt context, so we don't get a race
    // with waitForSendDone
    work->inProgress = false;
    Event(DEVICE_ID_DISPLAY, 101);
}

void ST7735::waitForSendDone()
{
    if (work->inProgress)
        fiber_wait_for_event(DEVICE_ID_DISPLAY, 101);
}

void ST7735::expandPalette(uint32_t *srcPalette, uint32_t *dstPalette)
{
    for (int i = 0; i < 256; ++i)
    {
        uint32_t p0 = color444(srcPalette[i >> 4]);
        uint32_t p1 = color444(srcPalette[i & 0xf]);
        uint32_t p = ((p0 << 12) | p1) << 8;
        dstPalette[i] = (p << 24) | ((p << 8) & 0xff0000) | ((p >> 8) & 0xff00) | (p >> 24);
    }
}

#define PAL8TO4(p) (((p >> 4) & 0xf) | ((p >> 8) & 0xf0) | ((p >> 12) & 0xf00))
int ST7735::sendIndexedImage(const uint8_t *src, unsigned numBytes, uint32_t *expPalette)
{
    if (!work)
    {
        work = new ST7735WorkBuffer;
        memset(work, 0, sizeof(*work));
        EventModel::defaultEventBus->listen(DEVICE_ID_DISPLAY, 100, this, &ST7735::sendDone);
    }

    if (work->inProgress)
        return DEVICE_BUSY;

    work->inProgress = true;
    work->paletteTable = expPalette;
    work->srcLeft = numBytes;
    work->srcPtr = src;

    cmdBuf[0] = ST7735_RAMWR;
    sendCmd(cmdBuf, 1);

    dc.setDigitalValue(1);
    cs.setDigitalValue(0);

    sendColorsStep(this);

    return DEVICE_OK;
}

// we don't modify *buf, but it cannot be in flash, so no const as a hint
void ST7735::sendCmd(uint8_t *buf, int len)
{
    // make sure cmd isn't on stack
    if (buf != cmdBuf)
        memcpy(cmdBuf, buf, len);
    buf = cmdBuf;
    dc.setDigitalValue(0);
    cs.setDigitalValue(0);
    spi.transfer(buf, 1, NULL, 0);
    dc.setDigitalValue(1);
    len--;
    buf++;
    if (len > 0)
        spi.transfer(buf, len, NULL, 0);
    cs.setDigitalValue(1);
}

void ST7735::sendCmdSeq(const uint8_t *buf)
{
    while (*buf)
    {
        cmdBuf[0] = *buf++;
        int v = *buf++;
        int len = v & ~DELAY;
        // note that we have to copy to RAM
        memcpy(cmdBuf + 1, buf, len);
        sendCmd(cmdBuf, len + 1);
        buf += len;
        if (v & DELAY)
        {
            fiber_sleep(*buf++);
        }
    }
}

void ST7735::setAddrWindow(int x, int y, int w, int h)
{
    x += 3;
    y += 2;
    uint8_t cmd0[] = {ST7735_CASET, 0, (uint8_t)x, 0, (uint8_t)(x + w - 1)};
    uint8_t cmd1[] = {ST7735_RASET, 0, (uint8_t)y, 0, (uint8_t)(y + h - 1)};
    sendCmd(cmd0, sizeof(cmd0));
    sendCmd(cmd1, sizeof(cmd1));
}

void ST7735::sendColors(const void *colors, int byteSize)
{
    cmdBuf[0] = ST7735_RAMWR;
    sendCmd(cmdBuf, 1);

    dc.setDigitalValue(1);
    cs.setDigitalValue(0);
    const uint8_t *ptr = (const uint8_t *)colors;
    while (byteSize)
    {
        int len = 254;
        if (byteSize < len)
            len = byteSize;
        spi.transfer(ptr, len, NULL, 0);
        ptr += len;
        byteSize -= len;
    }
    cs.setDigitalValue(1);
}

void ST7735::init()
{
    spi.setFrequency(8000000);
    spi.setMode(0);

    cs.setDigitalValue(1);
    dc.setDigitalValue(1);

    fiber_sleep(10);
    sendCmdSeq(initCmds);
}
}
#include "Sprite.h"
#include "CodalDmesg.h"

using namespace codal;

int Sprite::fill(uint8_t colour)
{
    DMESG("FILL");
    memset(image.getBitmap(), ((colour & 0x0f) << 4 | (colour & 0x0f)), image.getSize());
    return DEVICE_OK;
}


Sprite::Sprite(PhysicsBody& body, Image& i) : body(body)
{
    this->image = i;
}

int Sprite::setImage(Image& newImage)
{
    this->image = newImage;
    return DEVICE_OK;
}

Image Sprite::getImage()
{
    return this->image;
}

int Sprite::drawCircle(uint16_t x, uint16_t y, int radius, int colour)
{

}

int Sprite::drawRectangle(uint16_t x, uint16_t y, int width, int height, int colour)
{
    if (width == 0 || height == 0 || x >= image.getWidth() || y >= image.getHeight())
        return DEVICE_INVALID_PARAMETER;

    int x2 = x + width - 1;
    int y2 = y + height - 1;

    if (x2 < 0 || y2 < 0)
        return DEVICE_INVALID_PARAMETER;

    width = x2 - x + 1;
    height = y2 - y + 1;

    if (x == 0 && y == 0 && width == image.getWidth() && height == image.getHeight())
    {
        fill(colour);
        return DEVICE_OK;
    }

    // auto bh = image.byteHeight();
    // uint8_t f = image.fillMask(c);

    // uint8_t *p = image.pix(x, y);
    // while (w-- > 0) {
    //     if (image.bpp() == 1) {
    //         auto ptr = p;
    //         unsigned mask = 0x01 << (y & 7);

    //         for (int i = 0; i < h; ++i) {
    //             if (mask == 0x100) {
    //                 if (h - i >= 8) {
    //                     *++ptr = f;
    //                     i += 7;
    //                     continue;
    //                 } else {
    //                     mask = 0x01;
    //                     ++ptr;
    //                 }
    //             }
    //             if (c)
    //                 *ptr |= mask;
    //             else
    //                 *ptr &= ~mask;
    //             mask <<= 1;
    //         }

    //     } else if (image.bpp() == 4) {
    //         auto ptr = p;
    //         unsigned mask = 0x0f;
    //         if (y & 1)
    //             mask <<= 4;

    //         for (int i = 0; i < h; ++i) {
    //             if (mask == 0xf00) {
    //                 if (h - i >= 2) {
    //                     *++ptr = f;
    //                     i++;
    //                     continue;
    //                 } else {
    //                     mask = 0x0f;
    //                     ptr++;
    //                 }
    //             }
    //             *ptr = (*ptr & ~mask) | (f & mask);
    //             mask <<= 4;
    //         }
    //     }
    //     p += bh;
    // }
}

void Sprite::draw(Image& display)
{
    // DMESG("DRAW x: %d y: %d z: %d",  body.position.x, body.position.y, body.position.z);
    display.paste(this->image, body.position.x, body.position.y);
}
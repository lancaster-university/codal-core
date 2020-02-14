#include "JDCRC.h"

// Optimized implementation of CRC-CCITT (polynomial 0x1021)
// based on https://wiki.nicksoft.info/mcu:pic16:crc-16:home
// About 10 cycles per byte on M0+ (~40uS per packet)
uint16_t codal::jd_crc(uint8_t *data, uint32_t size)
{
    const uint8_t *ptr = (const uint8_t *)data;
    uint16_t crc = 0xffff;
    while (size--) {
        uint8_t data = *ptr++;
        uint8_t x = (crc >> 8) ^ data;
        x ^= x >> 4;
        crc = (crc << 8) ^ (x << 12) ^ (x << 5) ^ x;
    }
    return crc;
}

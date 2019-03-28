#include "JDCRC.h"

uint16_t codal::jd_crc(uint8_t *data, uint32_t len, JDDevice* device)
{
    uint16_t crc = 0xfff;

    int i = 0;
    if (device != NULL)
    {
        uint8_t* udidPtr = (uint8_t*)&device->unique_device_identifier;
        while (i < 8)
        {
            crc ^= (*udidPtr++ << 8);
            for (int i = 0; i < 8; ++i)
            {
                if (crc & 0x800)
                    crc = crc << 1 ^ JD_CRC_POLYNOMIAL;
                else
                    crc = crc << 1;
            }
            i++;
        }
    }

    while (len--)
    {
        crc ^= (*data++ << 8);
        for (int i = 0; i < 8; ++i)
        {
            if (crc & 0x800)
                crc = crc << 1 ^ JD_CRC_POLYNOMIAL;
            else
                crc = crc << 1;
        }
    }

    return crc & 0xFFF;
}


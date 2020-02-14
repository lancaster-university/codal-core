#include "JDDeviceManager.h"

#ifndef JD_CRC_H
#define JD_CRC_H

#define JD_CRC_POLYNOMIAL           0x1021

namespace codal
{
    /**
     * Computes the crc for a given buffer (data) and length (len)
     *
     * @param data the pointer to the data buffer
     * @param len the size of data
     *
     * @return the computed crc.
     **/
    uint16_t jd_crc(uint8_t *data, uint32_t len);
}

#endif
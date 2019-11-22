#include "JDDeviceManager.h"

#ifndef JD_CRC_H
#define JD_CRC_H

#define JD_CRC_POLYNOMIAL           0xF13

namespace codal
{
    /**
     * Computes the crc for a given buffer (data) and length (len)
     *
     * @param data the pointer to the data buffer
     * @param len the size of data
     * @param device the JDDevice to computer the crc with. The unique device identifier is used
     *        when calculating the crc destined for specific services. Device can be NULL, this indicates
     *        that the crc should not include a udid.
     *
     * @return the computed crc.
     **/
    uint16_t jd_crc(uint8_t *data, uint32_t len, JDDevice* device);
}

#endif
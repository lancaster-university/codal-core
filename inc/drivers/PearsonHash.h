#ifndef CODAL_PEARSON_HASH_H
#define CODAL_PEARSON_HASH_H

#include "CodalConfig.h"
#include "ManagedString.h"

namespace codal
{
    class PearsonHash
    {
        public:
        static uint8_t hash8(ManagedString s);
        static uint16_t hash16(ManagedString s);
    };
}

#endif
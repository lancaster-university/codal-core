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

#include "stdint.h"

#ifndef CODAL_LOADABLE_H
#define CODAL_LOADABLE_H

namespace codal {

    enum CodalCap {
        TIME = 1,
        STORAGE = 2
    };

    class CodalLoadable
    {
        public:
            uint16_t        cap_type;
            uint8_t         cap_priority;
            CodalLoadable * cap_next;

            CodalLoadable( uint16_t capabilityMask, uint8_t priority = 0xFF );

            static CodalLoadable * assign( CodalLoadable * current, CodalLoadable * candidate, bool chain = false );
    };
}

#endif
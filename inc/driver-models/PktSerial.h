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

#ifndef CODAL_PKTSERIAL_H
#define CODAL_PKTSERIAL_H

#include "CodalConfig.h"
#include "ErrorNo.h"
#include "Pin.h"
#include "Event.h"
#include "DMASingleWireSerial.h"

#define PKT_SERIAL_MAX_BUFFERS      10

// 2 bytes for size
#define PKT_SERIAL_HEADER_SIZE      4

#define PKT_SERIAL_RECEIVING        0x01
#define PKT_SERIAL_TRANSMITTING     0x02


#define PKT_SERIAL_DATA_READY       1

namespace codal
{

    struct PktSerialPkt {
        public:
        uint16_t size; // not including 'size' field
        uint16_t crc;
        // add more stuff
        uint8_t data[0];

        static PktSerialPkt *allocate(PktSerialPkt& p);
        static PktSerialPkt *allocate(uint16_t size);
    };
    /**
    * Class definition for a PktSerial interface.
    */
    class PktSerial : public CodalComponent
    {

        uint8_t bufferTail;

    protected:
        DMASingleWireSerial&  sws;
        Pin& sp;

        void onRisingEdge(Event);
        void dmaComplete(Event evt);
        int queuePacket();

    public:

        PktSerialPkt* rxBuf;
        PktSerialPkt* packetBuffer[PKT_SERIAL_MAX_BUFFERS];

        uint16_t id;

        PktSerial(Pin& p, DMASingleWireSerial& sws, uint16_t id = DEVICE_ID_PKTSERIAL0);

        PktSerialPkt *getPacket();

        /**
        * Start to listen.
        */
        virtual void start();

        /**
        * Disables protocol.
        */
        virtual void stop();

        /**
        * Writes to the PktSerial bus. Waits (possibly un-scheduled) for transfer to finish.
        */
        virtual int send(const PktSerialPkt *pkt);

        virtual int send(uint8_t* buf, int len);
    };
} // namespace codal

#endif

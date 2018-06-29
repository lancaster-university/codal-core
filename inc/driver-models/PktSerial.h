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

#define PKT_SERIAL_MAX_BUFFERS          10

#define PKT_SERIAL_RECEIVING            0x02
#define PKT_SERIAL_TRANSMITTING         0x04
#define PKT_SERIAL_TX_DRAIN_ENABLE      0x08


#define PKT_SERIAL_EVT_DATA_READY       1
#define PKT_SERIAL_EVT_BUS_ERROR        2
#define PKT_SERIAL_EVT_DRAIN            3

#define PKT_SERIAL_PACKET_SIZE          32
#define PKT_SERIAL_HEADER_SIZE          4
#define PKT_SERIAL_DATA_SIZE            PKT_SERIAL_PACKET_SIZE - PKT_SERIAL_HEADER_SIZE

#define PKT_SERIAL_MAXIMUM_BUFFERS      10

#define PKT_SERIAL_DMA_TIMEOUT          2   // 2 callback ~8 ms

#define PKT_PKT_FLAGS_LOSSY             0x01

namespace codal
{

    struct PktSerialPkt {
        public:
        uint16_t crc;
        uint8_t size; // not including anything before data
        uint8_t device_class;
        uint8_t device_id:4,flags:4;

        // add more stuff
        uint8_t data[PKT_SERIAL_DATA_SIZE];

        PktSerialPkt* next;
    } __attribute((__packed__));
    /**
    * Class definition for a PktSerial interface.
    */
    class PktSerial : public CodalComponent
    {
    protected:
        DMASingleWireSerial&  sws;
        Pin& sp;

        uint8_t timeoutCounter;

        void onFallingEdge(Event);
        void onRisingEdge(Event);
        void configure(bool events);
        void dmaComplete(Event evt);

        PktSerialPkt* popQueue(PktSerialPkt** queue);
        int addToQueue(PktSerialPkt** queue, PktSerialPkt* packet);
        PktSerialPkt* removeFromQueue(PktSerialPkt** queue, uint8_t device_class);

        void sendPacket(Event);

    public:
        PktSerialPkt* rxBuf;
        PktSerialPkt* txBuf;

        PktSerialPkt* rxQueue;
        PktSerialPkt* txQueue;

        uint16_t id;

        PktSerial(Pin& p, DMASingleWireSerial& sws, uint16_t id = DEVICE_ID_PKTSERIAL0);

        PktSerialPkt *getPacket();

        virtual void periodicCallback();

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
        virtual int send(PktSerialPkt *pkt);

        virtual int send(uint8_t* buf, int len);
    };
} // namespace codal

#endif

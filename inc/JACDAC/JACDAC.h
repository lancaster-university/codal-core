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

#ifndef CODAL_JACDAC_H
#define CODAL_JACDAC_H

#include "CodalConfig.h"
#include "ErrorNo.h"
#include "Pin.h"
#include "Event.h"
#include "DMASingleWireSerial.h"

#define JD_SERIAL_MAX_BUFFERS          10

#define JD_SERIAL_RECEIVING            0x02
#define JD_SERIAL_TRANSMITTING         0x04
#define JD_SERIAL_TX_DRAIN_ENABLE      0x08


#define JD_SERIAL_EVT_DATA_READY       1
#define JD_SERIAL_EVT_BUS_ERROR        2
#define JD_SERIAL_EVT_DRAIN            3

#define JD_SERIAL_HEADER_SIZE          4
#define JD_SERIAL_DATA_SIZE            32
#define JD_SERIAL_PACKET_SIZE          (JD_SERIAL_HEADER_SIZE + JD_SERIAL_DATA_SIZE)

#define JD_SERIAL_MAXIMUM_BUFFERS      10

#define JD_SERIAL_DMA_TIMEOUT          2   // 2 callback ~8 ms

#define JD_JD_FLAGS_LOSSY             0x01

#if CONFIG_ENABLED(JD_DEBUG)
#define JD_DMESG      codal_dmesg
#else
#define JD_DMESG(...) ((void)0)
#endif

namespace codal
{

    struct JDPkt {
        public:
        uint16_t crc;
        uint8_t address; // control is 0, devices are allocated address in the range 1 - 255
        uint8_t size;

        // add more stuff
        uint8_t data[JD_SERIAL_DATA_SIZE];

        JDPkt* next;
    } __attribute((__packed__));
    /**
    * Class definition for a JACDAC interface.
    */
    class JACDAC : public CodalComponent
    {
    protected:
        DMASingleWireSerial&  sws;
        Pin& sp;

        uint8_t timeoutCounter;
        uint8_t timeoutValue;

        void onFallingEdge(Event);
        void configure(bool events);
        void dmaComplete(Event evt);

        JDPkt* popQueue(JDPkt** queue);
        int addToQueue(JDPkt** queue, JDPkt* packet);
        JDPkt* removeFromQueue(JDPkt** queue, uint8_t device_class);

        void sendPacket(Event);

    public:
        JDPkt* rxBuf;
        JDPkt* txBuf;

        JDPkt* rxQueue;
        JDPkt* txQueue;

        uint16_t id;

        virtual void periodicCallback();

        /**
          * Constructor
          *
          * @param p the transmission pin to use
          *
          * @param sws an instance of sws created using p.
          */
        JACDAC(Pin& p, DMASingleWireSerial& sws, uint16_t id = DEVICE_ID_JACDAC0);

        /**
          * Retrieves the first packet on the rxQueue irregardless of the device_class
          *
          * @returns the first packet on the rxQueue or NULL
          */
        JDPkt *getPacket();

        /**
          * Retrieves the first packet on the rxQueue with a matching device_class
          *
          * @param address the address filter to apply to packets in the rxQueue
          *
          * @returns the first packet on the rxQueue matching the device_class or NULL
          */
        JDPkt* getPacket(uint8_t address);

        /**
          * Causes this instance of JACDAC to begin listening for packets transmitted on the serial line.
          */
        virtual void start();

        /**
          * Causes this instance of JACDAC to stop listening for packets transmitted on the serial line.
          */
        virtual void stop();

        /**
          * Sends a packet using the SingleWireSerial instance. This function begins the asynchronous transmission of a packet.
          * If an ongoing asynchronous transmission is happening, JD is added to the txQueue. If this is the first packet in a while
          * asynchronous transmission is begun.
          *
          * @param JD the packet to send.
          *
          * @returns DEVICE_OK on success, DEVICE_INVALID_PARAMETER if JD is NULL, or DEVICE_NO_RESOURCES if the queue is full.
          */
        virtual int send(JDPkt *JD);

        /**
          * Sends a packet using the SingleWireSerial instance. This function begins the asynchronous transmission of a packet.
          * If an ongoing asynchronous transmission is happening, JD is added to the txQueue. If this is the first packet in a while
          * asynchronous transmission is begun.
          *
          * @param buf the buffer to send.
          *
          * @param len the length of the buffer to send.
          *
          * @returns DEVICE_OK on success, DEVICE_INVALID_PARAMETER if buf is NULL or len is invalid, or DEVICE_NO_RESOURCES if the queue is full.
          */
        virtual int send(uint8_t* buf, int len, uint8_t address);

        bool isRunning();
    };
} // namespace codal

#endif

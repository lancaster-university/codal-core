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

#define JACDAC_VERSION                 1

#define JD_SERIAL_MAX_BUFFERS          10

#define JD_SERIAL_RECEIVING            0x02
#define JD_SERIAL_TRANSMITTING         0x04
#define JD_SERIAL_TX_DRAIN_ENABLE      0x08
#define JD_SERIAL_BUS_RISE             0x10


#define JD_SERIAL_EVT_DATA_READY       1
#define JD_SERIAL_EVT_BUS_ERROR        2
#define JD_SERIAL_EVT_DRAIN            3
#define JD_SERIAL_EVT_RX_TIMEOUT       4

#define JD_SERIAL_HEADER_SIZE          4
#define JD_SERIAL_DATA_SIZE            32
#define JD_SERIAL_PACKET_SIZE          (JD_SERIAL_HEADER_SIZE + JD_SERIAL_DATA_SIZE)

#define JD_SERIAL_MAXIMUM_BUFFERS      10

#define JD_SERIAL_DMA_TIMEOUT          2   // 2 callback ~8 ms

#define JD_SERIAL_MAX_BAUD             1000000
#define JD_SERIAL_TX_MAX_BACKOFF       4000
#define JD_SERIAL_TX_MIN_BACKOFF       1000

#if CONFIG_ENABLED(JD_DEBUG)
#define JD_DMESG      codal_dmesg
#else
#define JD_DMESG(...) ((void)0)
#endif

namespace codal
{
    /**
     * A JDPkt contains: a crc for error checking, the size of the packet
     * and an address.
     *
     * An address of a packet can be the drivers own address, or another drivers address.
     *
     * Why would you send a packet using your own address? Well, there are a number of different paradigms in JACDAC:
     *
     * 1) Virtual Drivers
     *
     * Virtual drivers allow the virtualisation of a remote resource. Packets produced from the remote resource are
     * consumed as if the remote resource is on the device. An example of this in action would be a remote Accelerometer.
     * Any device can modify the configuration of the remote resource. Only the host of the remote resource is required to be enumerated.
     *
     * 2) Paired Drivers
     *
     * Sharing resources is great, except when you want to own a resource like a joystick. Paired drivers allow ownership
     * over a remote resource. This mode requires both the host and the consumer to enumerated, so that one can tell if
     * the other disappears from the bus and vice versa.
     *
     * 3) Broadcast Drivers
     *
     * Sometimes drivers would like to receive all packets on the bus and do not require the use of addresses. This mode
     * allows any packet addressed to a class to be received. Broadcast drivers can be combined with either mode above,
     * or none at all.
     **/
    struct JDPkt {
        uint16_t crc;
        uint8_t address; // control is 0, devices are allocated address in the range 1 - 255
        uint8_t size;

        // add more stuff
        uint8_t data[JD_SERIAL_DATA_SIZE];

        JDPkt* next;
    } __attribute((__packed__));

    enum class JACDACBusState : uint8_t
    {
        Receiving,
        Transmitting,
        High,
        Low
    };

    /**
     * This enumeration defines the low time of the tx pulse, and the transmission speed of
     * this JACDAC device on the bus.
     **/
    enum class JACDACBaudRate : uint8_t
    {
        Baud1M = 1,
        Baud500K = 2,
        Baud250K = 4,
        Baud125K = 8
    };

    /**
    * Class definition for a JACDAC interface.
    */
    class JACDAC : public CodalComponent
    {
        JACDACBaudRate baud;

    protected:
        DMASingleWireSerial&  sws;
        Pin&  sp;

        void onFallingEdge(Event);
        void configure(bool events);
        void dmaComplete(Event evt);

        JDPkt* popQueue(JDPkt** queue);
        int addToQueue(JDPkt** queue, JDPkt* packet);
        JDPkt* removeFromQueue(JDPkt** queue, uint8_t device_class);

        void sendPacket(Event);

        void rxTimeout(Event);

    public:
        JDPkt* rxBuf;
        JDPkt* txBuf;

        JDPkt* rxQueue;
        JDPkt* txQueue;

        /**
          * Constructor
          *
          * @param sws an instance of sws.
          *
          * @param baud Defaults to 1mbaud
          */
        JACDAC(DMASingleWireSerial&  sws, JACDACBaudRate baud = JACDACBaudRate::Baud1M, uint16_t id = DEVICE_ID_JACDAC0);

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

        /**
         * Returns a bool indicating whether the JACDAC driver has been started.
         *
         * @return true if started, false if not.
         **/
        bool isRunning();

        /**
         * Returns the current state of the bus, either:
         *
         * * Receiving if the driver is in the process of receiving a packet.
         * * Transmitting if the driver is communicating a packet on the bus.
         *
         * If neither of the previous states are true, then the driver looks at the bus and returns the bus state:
         *
         * * High, if the line is currently floating high.
         * * Lo if something is currently pulling the line low.
         **/
        JACDACBusState getState();

        int setBaud(JACDACBaudRate baudRate);

        JACDACBaudRate getBaud();
    };
} // namespace codal

#endif

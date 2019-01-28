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
#include "LowLevelTimer.h"

#define JD_VERSION                     0

// various timings in microseconds
// 8 data bits, 1 start bit, 1 stop bit.
#define JD_BYTE_AT_125KBAUD                                 80
// the maximum permitted time between bytes
#define JD_MAX_INTERBYTE_SPACING                            (2 * JD_BYTE_AT_125KBAUD)
// the minimum permitted time between the data packets
#define JD_MIN_INTERFRAME_SPACING                           (2 * JD_BYTE_AT_125KBAUD)
// the time it takes for the bus to be considered in a normal state
#define JD_BUS_NORMALITY_PERIOD                             (2 * JD_BYTE_AT_125KBAUD)
// the minimum permitted time between the low pulse and data being received is 2 times 1 byte at the current baud rate.
#define JD_INTERLODATA_SPACING_MULTIPLIER                   2
// max spacing is 3 times 1 byte at minimum baud rate (240 us)
#define JD_MAX_INTERLODATA_SPACING                          (3 * JD_BYTE_AT_125KBAUD)

#define JD_SERIAL_MAX_BUFFERS          10

#define JD_SERIAL_RECEIVING             0x0002
#define JD_SERIAL_RECEIVING_HEADER      0x0004
#define JD_SERIAL_TRANSMITTING          0x0008
#define JD_SERIAL_TX_DRAIN_ENABLE       0x0010

#define JD_SERIAL_BUS_LO_ERROR          0x0020
#define JD_SERIAL_BUS_TIMEOUT_ERROR     0x0040
#define JD_SERIAL_BUS_UART_ERROR        0x0080
#define JD_SERIAL_ERR_MSK               0x00E0

#define JD_SERIAL_BUS_STATE             0x0100
#define JD_SERIAL_BUS_TOGGLED           0x0200
#define JD_SERIAL_LO_PULSE_START        0x0400

#define JD_SERIAL_EVT_DATA_READY       1
#define JD_SERIAL_EVT_BUS_ERROR        2
#define JD_SERIAL_EVT_CRC_ERROR        3
#define JD_SERIAL_EVT_DRAIN            4
#define JD_SERIAL_EVT_RX_TIMEOUT       5

#define JD_SERIAL_EVT_BUS_CONNECTED    5
#define JD_SERIAL_EVT_BUS_DISCONNECTED 6

#define JD_SERIAL_HEADER_SIZE          5
#define JD_SERIAL_CRC_HEADER_SIZE      2  // when computing CRC, we skip the CRC and version fields, so the header size decreases by three.
#define JD_SERIAL_MAX_PAYLOAD_SIZE     255

#define JD_SERIAL_MAXIMUM_BUFFERS      10

#define JD_SERIAL_DMA_TIMEOUT          2   // 2 callback ~8 ms

#define JD_SERIAL_MAX_BAUD             1000000
#define JD_SERIAL_TX_MAX_BACKOFF       1000

#define JD_RX_ARRAY_SIZE               10
#define JD_TX_ARRAY_SIZE               10

#if CONFIG_ENABLED(JD_DEBUG)
#define JD_DMESG      codal_dmesg
#else
#define JD_DMESG(...) ((void)0)
#endif

namespace codal
{
    /**
     * A JDPacket contains: a crc for error checking, the size of the packet
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
    struct JDPacket {
        uint8_t jacdac_version; // identifies the version of the entire stack, filled out by the JACDAC driver
        uint16_t crc; // a cyclic redundancy check, filled out by the JACDAC driver (currently Fletcher16 is used).
        uint8_t address; // control is 0, devices are allocated address in the range 1 - 255
        uint8_t size; // the size, address, and crc are not included by the size variable. The size of a packet dictates the size of the data field.
        // add more stuff
        uint8_t data[JD_SERIAL_MAX_PAYLOAD_SIZE];
        uint8_t communication_rate;
    } __attribute((__packed__));

    enum class JDBusState : uint8_t
    {
        Receiving,
        Transmitting,
        High,
        Low
    };

    enum class JDSerialState : uint8_t
    {
        ListeningForPulse,
        ErrorRecovery,
        Off
    };

    /**
     * This enumeration defines the low time of the tx pulse, and the transmission speed of
     * this JACDAC device on the bus.
     **/
    enum class JDBaudRate : uint8_t
    {
        Baud1M = 1,
        Baud500K = 2,
        Baud250K = 4,
        Baud125K = 8
    };

    enum JDBusErrorState : uint16_t
    {
        Continuation = 0,
        BusLoError = JD_SERIAL_BUS_LO_ERROR,
        BusTimeoutError = JD_SERIAL_BUS_TIMEOUT_ERROR,
        BusUARTError = BusTimeoutError // a different error code, but we want the same behaviour.
    };

    /**
    * Class definition for a JACDAC interface.
    */
    class JACDAC : public CodalComponent
    {
        JDBaudRate maxBaud;
        JDBaudRate currentBaud;
        uint8_t bufferOffset;

    protected:
        DMASingleWireSerial&  sws;
        Pin&  sp;
        LowLevelTimer& timer;

        Pin* busLED;
        Pin* commLED;

        JDSerialState state;

        uint32_t startTime;
        uint32_t lastBufferedCount;


        void loPulseDetected(uint32_t);
        void setState(JDSerialState s);
        void dmaComplete(Event evt);

        JDPacket* popRxArray();
        JDPacket* popTxArray();
        int addToTxArray(JDPacket* packet);
        int addToRxArray(JDPacket* packet);
        void sendPacket();
        void initialise();
        void errorState(JDBusErrorState);

    public:

        static JACDAC* instance;

        uint8_t txHead;
        uint8_t txTail;
        uint8_t rxHead;
        uint8_t rxTail;

        JDPacket* rxBuf; // holds the pointer to the current rx buffer
        JDPacket* txBuf; // holds the pointer to the current tx buffer
        JDPacket* rxArray[JD_RX_ARRAY_SIZE];
        JDPacket* txArray[JD_TX_ARRAY_SIZE];

        /**
          * Constructor
          *
          * @param sws an instance of sws.
          *
          * @param busStateLED an instance of a pin, used to display the state of the bus.
          *
          * @param commStateLED an instance of a pin, used to display the state of the bus.
          *
          * @param baud Defaults to 1mbaud
          */
        JACDAC(DMASingleWireSerial&  sws, LowLevelTimer& timer, Pin* busStateLED = NULL, Pin* commStateLED = NULL, JDBaudRate baud = JDBaudRate::Baud1M, uint16_t id = DEVICE_ID_JACDAC0);

        /**
          * Retrieves the first packet on the rxQueue irregardless of the device_class
          *
          * @returns the first packet on the rxQueue or NULL
          */
        JDPacket *getPacket();

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
          * @param compute_crc default = true. When true, the crc is calculated in this member function... if set to false, the crc field is left untouched.
          *
          * @returns DEVICE_OK on success, DEVICE_INVALID_PARAMETER if JD is NULL, or DEVICE_NO_RESOURCES if the queue is full.
          */
        virtual int send(JDPacket *p, bool compute_crc = true);

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
        virtual int send(uint8_t* buf, int len, uint8_t address, JDBaudRate communicationRate);

        /**
         * Returns a bool indicating whether the JACDAC driver has been started.
         *
         * @return true if started, false if not.
         **/
        bool isRunning();

        /**
         * Returns the current state if the bus.
         *
         * @return true if connected, false if there's a bad bus condition.
         **/
        bool isConnected();

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
        JDBusState getState();

        /**
         * Sets the maximum baud rate which is used as a default (if no communication rate is given in any packet), and as
         * a maximum reception rate.
         *
         * @param baudRate the desired baud rate for this jacdac instance, one of: Baud1M, Baud500K, Baud250K, Baud125K
         *
         * @returns DEVICE_OK on success
         **/
        int setMaximumBaud(JDBaudRate baudRate);

        /**
         * Returns the current maximum baud rate.
         *
         * @returns the enumerated baud rate for this jacdac instance, one of: Baud1M, Baud500K, Baud250K, Baud125K
         **/
        JDBaudRate getMaximumBaud();

        void _timerCallback(uint16_t channels);
        void _gpioCallback(int state);
    };
} // namespace codal

#endif

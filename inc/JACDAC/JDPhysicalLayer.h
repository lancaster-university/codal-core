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

#ifndef CODAL_JD_PHYSICAL_H
#define CODAL_JD_PHYSICAL_H

#include "CodalConfig.h"
#include "ErrorNo.h"
#include "Pin.h"
#include "Event.h"
#include "DMASingleWireSerial.h"
#include "LowLevelTimer.h"
#include "JDDeviceManager.h"

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
// the minimum permitted time between the low pulse and data being received is 40 us
#define JD_MIN_INTERLODATA_SPACING                          40
// max spacing is 3 times 1 byte at minimum baud rate (240 us)
#define JD_MAX_INTERLODATA_SPACING                          (3 * JD_BYTE_AT_125KBAUD)

#define JD_SERIAL_MAX_BUFFERS           10

#define JD_SERIAL_MAX_SERVICE_NUMBER    15

#define JD_SERIAL_RECEIVING             0x0001
#define JD_SERIAL_RECEIVING_HEADER      0x0002
#define JD_SERIAL_TRANSMITTING          0x0004
#define JD_SERIAL_RX_LO_PULSE           0x0008
#define JD_SERIAL_TX_LO_PULSE           0x0010

#define JD_SERIAL_BUS_LO_ERROR          0x0020
#define JD_SERIAL_BUS_TIMEOUT_ERROR     0x0040
#define JD_SERIAL_BUS_UART_ERROR        0x0080
#define JD_SERIAL_ERR_MSK               0x00E0

#define JD_SERIAL_BUS_STATE             0x0100
#define JD_SERIAL_BUS_TOGGLED           0x0200

#define JD_SERIAL_DEBUG_BIT             0x8000

#define JD_SERIAL_EVT_DATA_READY       1
#define JD_SERIAL_EVT_BUS_ERROR        2
#define JD_SERIAL_EVT_CRC_ERROR        3
#define JD_SERIAL_EVT_DRAIN            4
#define JD_SERIAL_EVT_RX_TIMEOUT       5

#define JD_SERIAL_EVT_BUS_CONNECTED    5
#define JD_SERIAL_EVT_BUS_DISCONNECTED 6

#define JD_SERIAL_HEADER_SIZE          4
#define JD_SERIAL_CRC_HEADER_SIZE      2  // when computing CRC, we skip the CRC and version fields, so the header size decreases by three.
#define JD_SERIAL_MAX_PAYLOAD_SIZE     255

#define JD_SERIAL_MAXIMUM_BUFFERS      10

#define JD_SERIAL_DMA_TIMEOUT          2   // 2 callback ~8 ms

#define JD_SERIAL_MAX_BAUD             1000000
#define JD_SERIAL_TX_MAX_BACKOFF       1000

#ifndef JD_RX_ARRAY_SIZE
#define JD_RX_ARRAY_SIZE               10
#endif

#ifndef JD_TX_ARRAY_SIZE
#define JD_TX_ARRAY_SIZE               10
#endif

#define JD_SERIAL_BAUD_1M              1
#define JD_SERIAL_BAUD_500K            2
#define JD_SERIAL_BAUD_250K            4
#define JD_SERIAL_BAUD_125K            8

#if CONFIG_ENABLED(JD_DEBUG)
#define JD_DMESG      codal_dmesg
#else
#define JD_DMESG(...) ((void)0)
#endif

namespace codal
{
    class JDService;
    // a struct containing the various diagnostics of the JACDAC physical layer.
    struct JDDiagnostics
    {
        uint32_t bus_state;
        uint32_t bus_lo_error;
        uint32_t bus_uart_error;
        uint32_t bus_timeout_error;
        uint32_t packets_sent;
        uint32_t packets_received;
        uint32_t packets_dropped;
    };

    struct JDPacket
    {
        uint16_t crc:12, service_number:4; // crc is stored in the first 12 bits, service number in the final 4 bits
        uint8_t device_address; // control is 0, devices are allocated address in the range 1 - 255
        uint8_t size; // the size, address, and crc are not included by the size variable. The size of a packet dictates the size of the data field.
        uint8_t data[JD_SERIAL_MAX_PAYLOAD_SIZE];
        uint8_t communication_rate;
    } __attribute((__packed__));

    enum class JDBusState : uint8_t
    {
        Receiving,
        Transmitting,
        Error,
        Unknown
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
        Baud1M = JD_SERIAL_BAUD_1M,
        Baud500K = JD_SERIAL_BAUD_500K,
        Baud250K = JD_SERIAL_BAUD_250K,
        Baud125K = JD_SERIAL_BAUD_125K
    };

    enum JDBusErrorState : uint16_t
    {
        Continuation = 0,
        BusLoError = JD_SERIAL_BUS_LO_ERROR,
        BusTimeoutError = JD_SERIAL_BUS_TIMEOUT_ERROR,
        BusUARTError = JD_SERIAL_BUS_UART_ERROR // a different error code, but we want the same behaviour.
    };

    /**
    * Class definition for a JACDAC interface.
    */
    class JDPhysicalLayer : public CodalComponent
    {
        friend class USBJACDAC;

        JDBaudRate maxBaud;
        JDBaudRate currentBaud;
        uint8_t bufferOffset;

        JDService* sniffer;

    protected:
        DMASingleWireSerial&  sws;
        Pin&  sp;
        LowLevelTimer& timer;

        Pin* busLED;
        Pin* commLED;

        bool busLEDActiveLo;
        bool commLEDActiveLo;

        JDSerialState state;

        uint32_t startTime;
        uint32_t lastBufferedCount;

        void loPulseDetected(uint32_t);
        int setState(JDSerialState s);
        void dmaComplete(Event evt);

        JDPacket* popRxArray();
        JDPacket* popTxArray();
        int addToTxArray(JDPacket* packet);
        int addToRxArray(JDPacket* packet);
        void sendPacket();
        void errorState(JDBusErrorState);

        /**
          * Sends a packet using the SingleWireSerial instance. This function begins the asynchronous transmission of a packet.
          * If an ongoing asynchronous transmission is happening, JD is added to the txQueue. If this is the first packet in a while
          * asynchronous transmission is begun.
          *
          * @returns DEVICE_OK on success, DEVICE_INVALID_PARAMETER if JD is NULL, or DEVICE_NO_RESOURCES if the queue is full.
          */
        virtual int queuePacket(JDPacket *p);

    public:

        static JDPhysicalLayer* instance;

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
        JDPhysicalLayer(DMASingleWireSerial&  sws, LowLevelTimer& timer, Pin* busStateLED = NULL, Pin* commStateLED = NULL, bool busLEDActiveLo = false, bool commLEDActiveLo = false, JDBaudRate baud = JDBaudRate::Baud1M, uint16_t id = DEVICE_ID_JACDAC_PHYS);

        /**
          * Retrieves the first packet on the rxQueue regardless of the device_class
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

        int send(JDPacket* tx, JDDevice* device, bool computeCRC = true);

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
        int send(uint8_t* buf, int len, uint8_t service_number, JDDevice* device);

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

        uint8_t getErrorState();
        JDDiagnostics getDiagnostics();

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

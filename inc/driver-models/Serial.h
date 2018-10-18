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

#ifndef CODAL_SERIAL_H
#define CODAL_SERIAL_H

#include "ManagedString.h"
#include "Pin.h"

#define CODAL_SERIAL_DEFAULT_BAUD_RATE   115200
#define CODAL_SERIAL_DEFAULT_BUFFER_SIZE 20

#define CODAL_SERIAL_EVT_DELIM_MATCH     1
#define CODAL_SERIAL_EVT_HEAD_MATCH      2
#define CODAL_SERIAL_EVT_RX_FULL         3

#define CODAL_SERIAL_RX_IN_USE           1
#define CODAL_SERIAL_TX_IN_USE           2
#define CODAL_SERIAL_RX_BUFF_INIT        4
#define CODAL_SERIAL_TX_BUFF_INIT        8


namespace codal
{
    enum SerialMode
    {
        ASYNC,
    	SYNC_SPINWAIT,
    	SYNC_SLEEP
    };

    enum SerialInterruptType
    {
        RxInterrupt = 0,
        TxInterrupt
    };

    /**
      * Class definition for DeviceSerial.
      *
      * Represents an instance of RawSerial which accepts codal device specific data types.
      */
    class Serial
    {
        protected:

        //holds that state of the mutex locks for all DeviceSerial instances.
        static uint8_t status;

        //holds the state of the baudrate for all DeviceSerial instances.
        static int baudrate;

        //delimeters used for matching on receive.
        ManagedString delimeters;

        //a variable used when a user calls the eventAfter() method.
        int rxBuffHeadMatch;

        uint8_t *rxBuff;
        uint8_t rxBuffSize;
        volatile uint16_t rxBuffHead;
        uint16_t rxBuffTail;


        uint8_t *txBuff;
        uint8_t txBuffSize;
        uint16_t txBuffHead;
        volatile uint16_t txBuffTail;

        public:

        /**
          * Constructor.
          * Create an instance of DeviceSerial
          *
          * @param tx the Pin to be used for transmission
          *
          * @param rx the Pin to be used for receiving data
          *
          * @param rxBufferSize the size of the buffer to be used for receiving bytes
          *
          * @param txBufferSize the size of the buffer to be used for transmitting bytes
          *
          * @code
          * DeviceSerial serial(USBTX, USBRX);
          * @endcode
          * @note the default baud rate is 115200. More API details can be found:
          *       -https://github.com/mbedmicro/mbed/blob/master/libraries/mbed/api/SerialBase.h
          *       -https://github.com/mbedmicro/mbed/blob/master/libraries/mbed/api/RawSerial.h
          *
          *       Buffers aren't allocated until the first send or receive respectively.
          */
        Serial(Pin& tx, Pin& rx, uint8_t rxBufferSize = CODAL_SERIAL_DEFAULT_BUFFER_SIZE, uint8_t txBufferSize = CODAL_SERIAL_DEFAULT_BUFFER_SIZE)
        {
        }

        /**
          * Sends a single character over the serial line.
          *
          * @param c the character to send
          *
          * @param mode the selected mode, one of: ASYNC, SYNC_SPINWAIT, SYNC_SLEEP. Each mode
          *        gives a different behaviour:
          *
          *            ASYNC - the character is copied into the txBuff and returns immediately.
          *
          *            SYNC_SPINWAIT - the character is copied into the txBuff and this method
          *                            will spin (lock up the processor) until the character has
          *                            been sent.
          *
          *            SYNC_SLEEP - the character is copied into the txBuff and the fiber sleeps
          *                         until the character has been sent. This allows other fibers
          *                         to continue execution.
          *
          *         Defaults to SYNC_SLEEP.
          *
          * @return the number of bytes written, or CODAL_SERIAL_IN_USE if another fiber
          *         is using the serial instance for transmission.
          */
        virtual int sendChar(char c, SerialMode mode = DEVICE_DEFAULT_SERIAL_MODE)
        {
            return DEVICE_NOT_IMPLEMENTED;
        }

        /**
          * Sends a ManagedString over the serial line.
          *
          * @param s the string to send
          *
          * @param mode the selected mode, one of: ASYNC, SYNC_SPINWAIT, SYNC_SLEEP. Each mode
          *        gives a different behaviour:
          *
          *            ASYNC - bytes are copied into the txBuff and returns immediately.
          *
          *            SYNC_SPINWAIT - bytes are copied into the txBuff and this method
          *                            will spin (lock up the processor) until all bytes
          *                            have been sent.
          *
          *            SYNC_SLEEP - bytes are copied into the txBuff and the fiber sleeps
          *                         until all bytes have been sent. This allows other fibers
          *                         to continue execution.
          *
          *         Defaults to SYNC_SLEEP.
          *
          * @return the number of bytes written, CODAL_SERIAL_IN_USE if another fiber
          *         is using the serial instance for transmission, DEVICE_INVALID_PARAMETER
          *         if buffer is invalid, or the given bufferLen is <= 0.
          */
        virtual int send(ManagedString s, SerialMode mode = DEVICE_DEFAULT_SERIAL_MODE)
        {
            return DEVICE_NOT_IMPLEMENTED;
        }

        /**
          * Sends a buffer of known length over the serial line.
          *
          * @param buffer a pointer to the first character of the buffer
          *
          * @param len the number of bytes that are safely available to read.
          *
          * @param mode the selected mode, one of: ASYNC, SYNC_SPINWAIT, SYNC_SLEEP. Each mode
          *        gives a different behaviour:
          *
          *            ASYNC - bytes are copied into the txBuff and returns immediately.
          *
          *            SYNC_SPINWAIT - bytes are copied into the txBuff and this method
          *                            will spin (lock up the processor) until all bytes
          *                            have been sent.
          *
          *            SYNC_SLEEP - bytes are copied into the txBuff and the fiber sleeps
          *                         until all bytes have been sent. This allows other fibers
          *                         to continue execution.
          *
          *         Defaults to SYNC_SLEEP.
          *
          * @return the number of bytes written, CODAL_SERIAL_IN_USE if another fiber
          *         is using the serial instance for transmission, DEVICE_INVALID_PARAMETER
          *         if buffer is invalid, or the given bufferLen is <= 0.
          */
        virtual int send(uint8_t *buffer, int bufferLen, SerialMode mode = DEVICE_DEFAULT_SERIAL_MODE)
        {
            return DEVICE_NOT_IMPLEMENTED;
        }

        /**
          * Reads a single character from the rxBuff
          *
          * @param mode the selected mode, one of: ASYNC, SYNC_SPINWAIT, SYNC_SLEEP. Each mode
          *        gives a different behaviour:
          *
          *            ASYNC - A character is read from the rxBuff if available, if there
          *                    are no characters to be read, a value of DEVICE_NO_DATA is returned immediately.
          *
          *            SYNC_SPINWAIT - A character is read from the rxBuff if available, if there
          *                            are no characters to be read, this method will spin
          *                            (lock up the processor) until a character is available.
          *
          *            SYNC_SLEEP - A character is read from the rxBuff if available, if there
          *                         are no characters to be read, the calling fiber sleeps
          *                         until there is a character available.
          *
          *         Defaults to SYNC_SLEEP.
          *
          * @return a character, CODAL_SERIAL_IN_USE if another fiber is using the serial instance for reception,
          *         DEVICE_NO_RESOURCES if buffer allocation did not complete successfully, or DEVICE_NO_DATA if
          *         the rx buffer is empty and the mode given is ASYNC.
          */
        virtual int read(SerialMode mode = DEVICE_DEFAULT_SERIAL_MODE)
        {
            return DEVICE_NOT_IMPLEMENTED;
        }

        /**
          * Reads multiple characters from the rxBuff and returns them as a ManagedString
          *
          * @param size the number of characters to read.
          *
          * @param mode the selected mode, one of: ASYNC, SYNC_SPINWAIT, SYNC_SLEEP. Each mode
          *        gives a different behaviour:
          *
          *            ASYNC - If the desired number of characters are available, this will return
          *                    a ManagedString with the expected size. Otherwise, it will read however
          *                    many characters there are available.
          *
          *            SYNC_SPINWAIT - If the desired number of characters are available, this will return
          *                            a ManagedString with the expected size. Otherwise, this method will spin
          *                            (lock up the processor) until the desired number of characters have been read.
          *
          *            SYNC_SLEEP - If the desired number of characters are available, this will return
          *                         a ManagedString with the expected size. Otherwise, the calling fiber sleeps
          *                         until the desired number of characters have been read.
          *
          *         Defaults to SYNC_SLEEP.
          *
          * @return A ManagedString, or an empty ManagedString if an error was encountered during the read.
          */
        virtual ManagedString read(int size, SerialMode mode = DEVICE_DEFAULT_SERIAL_MODE)
        {
            return DEVICE_NOT_IMPLEMENTED;
        }

        /**
          * Reads multiple characters from the rxBuff and fills a user buffer.
          *
          * @param buffer a pointer to a user allocated buffer.
          *
          * @param bufferLen the amount of data that can be safely stored
          *
          * @param mode the selected mode, one of: ASYNC, SYNC_SPINWAIT, SYNC_SLEEP. Each mode
          *        gives a different behaviour:
          *
          *            ASYNC - If the desired number of characters are available, this will fill
          *                    the given buffer. Otherwise, it will fill the buffer with however
          *                    many characters there are available.
          *
          *            SYNC_SPINWAIT - If the desired number of characters are available, this will fill
          *                            the given buffer. Otherwise, this method will spin (lock up the processor)
          *                            and fill the buffer until the desired number of characters have been read.
          *
          *            SYNC_SLEEP - If the desired number of characters are available, this will fill
          *                         the given buffer. Otherwise, the calling fiber sleeps
          *                         until the desired number of characters have been read.
          *
          *         Defaults to SYNC_SLEEP.
          *
          * @return the number of characters read, or CODAL_SERIAL_IN_USE if another fiber
          *         is using the instance for receiving.
          */
        virtual int read(uint8_t *buffer, int bufferLen, SerialMode mode = DEVICE_DEFAULT_SERIAL_MODE)
        {
            return DEVICE_NOT_IMPLEMENTED;
        }

        /**
          * Reads until one of the delimeters matches a character in the rxBuff
          *
          * @param delimeters a ManagedString containing a sequence of delimeter characters e.g. ManagedString("\r\n")
          *
          * @param mode the selected mode, one of: ASYNC, SYNC_SPINWAIT, SYNC_SLEEP. Each mode
          *        gives a different behaviour:
          *
          *            ASYNC - If one of the delimeters matches a character already in the rxBuff
          *                    this method will return a ManagedString up to the delimeter.
          *                    Otherwise, it will return an Empty ManagedString.
          *
          *            SYNC_SPINWAIT - If one of the delimeters matches a character already in the rxBuff
          *                            this method will return a ManagedString up to the delimeter.
          *                            Otherwise, this method will spin (lock up the processor) until a
          *                            received character matches one of the delimeters.
          *
          *            SYNC_SLEEP - If one of the delimeters matches a character already in the rxBuff
          *                         this method will return a ManagedString up to the delimeter.
          *                         Otherwise, the calling fiber sleeps until a character matching one
          *                         of the delimeters is seen.
          *
          *         Defaults to SYNC_SLEEP.
          *
          * @return A ManagedString containing the characters up to a delimeter, or an Empty ManagedString,
          *         if another fiber is currently using this instance for reception.
          *
          * @note delimeters are matched on a per byte basis.
          */
        virtual ManagedString readUntil(ManagedString delimeters, SerialMode mode = DEVICE_DEFAULT_SERIAL_MODE)
        {
            return DEVICE_NOT_IMPLEMENTED;
        }

        /**
          * A wrapper around the inherited method "baud" so we can trap the baud rate
          * as it changes and restore it if redirect() is called.
          *
          * @param baudrate the new baudrate. See:
          *         - https://github.com/mbedmicro/mbed/blob/master/libraries/mbed/targets/hal/TARGET_NORDIC/TARGET_MCU_NRF51822/serial_api.c
          *        for permitted baud rates.
          *
          * @return DEVICE_INVALID_PARAMETER if baud rate is less than 0, otherwise DEVICE_OK.
          *
          * @note the underlying implementation chooses the first allowable rate at or above that requested.
          */
        virtual int baud(int baudrate)
        {
            return DEVICE_NOT_IMPLEMENTED;
        }

        /**
          * A way of dynamically configuring the serial instance to use pins other than USBTX and USBRX.
          *
          * @param tx the new transmission pin.
          *
          * @param rx the new reception pin.
          *
          * @return CODAL_SERIAL_IN_USE if another fiber is currently transmitting or receiving, otherwise DEVICE_OK.
          */
        virtual int redirect(Pin& tx, Pin& rx)
        {
            return DEVICE_NOT_IMPLEMENTED;
        }

        /**
          * Configures an event to be fired after "len" characters.
          *
          * Will generate an event with the ID: DEVICE_ID_SERIAL and the value CODAL_SERIAL_EVT_HEAD_MATCH.
          *
          * @param len the number of characters to wait before triggering the event.
          *
          * @param mode the selected mode, one of: ASYNC, SYNC_SPINWAIT, SYNC_SLEEP. Each mode
          *        gives a different behaviour:
          *
          *            ASYNC - Will configure the event and return immediately.
          *
          *            SYNC_SPINWAIT - will return DEVICE_INVALID_PARAMETER
          *
          *            SYNC_SLEEP - Will configure the event and block the current fiber until the
          *                         event is received.
          *
          * @return DEVICE_INVALID_PARAMETER if the mode given is SYNC_SPINWAIT, otherwise DEVICE_OK.
          */
        virtual int eventAfter(int len, SerialMode mode = ASYNC)
        {
            return DEVICE_NOT_IMPLEMENTED;
        }

        /**
          * Configures an event to be fired on a match with one of the delimeters.
          *
          * Will generate an event with the ID: DEVICE_ID_SERIAL and the value CODAL_SERIAL_EVT_DELIM_MATCH.
          *
          * @param delimeters the characters to match received characters against e.g. ManagedString("\n")
          *
          * @param mode the selected mode, one of: ASYNC, SYNC_SPINWAIT, SYNC_SLEEP. Each mode
          *        gives a different behaviour:
          *
          *            ASYNC - Will configure the event and return immediately.
          *
          *            SYNC_SPINWAIT - will return DEVICE_INVALID_PARAMETER
          *
          *            SYNC_SLEEP - Will configure the event and block the current fiber until the
          *                         event is received.
          *
          * @return DEVICE_INVALID_PARAMETER if the mode given is SYNC_SPINWAIT, otherwise DEVICE_OK.
          *
          * @note delimeters are matched on a per byte basis.
          */
        virtual int eventOn(ManagedString delimeters, SerialMode mode = ASYNC)
        {
            return DEVICE_NOT_IMPLEMENTED;
        }

        /**
          * Determines whether there is any data waiting in our Rx buffer.
          *
          * @return 1 if we have space, 0 if we do not.
          *
          * @note We do not wrap the super's readable() method as we don't want to
          *       interfere with communities that use manual calls to serial.readable().
          */
        virtual int isReadable()
        {
            return DEVICE_NOT_IMPLEMENTED;
        }

        /**
          * Determines if we have space in our txBuff.
          *
          * @return 1 if we have space, 0 if we do not.
          *
          * @note We do not wrap the super's writeable() method as we don't want to
          *       interfere with communities that use manual calls to serial.writeable().
          */
        virtual int isWriteable()
        {
            return DEVICE_NOT_IMPLEMENTED;
        }

        /**
          * Reconfigures the size of our rxBuff
          *
          * @param size the new size for our rxBuff
          *
          * @return CODAL_SERIAL_IN_USE if another fiber is currently using this instance
          *         for reception, otherwise DEVICE_OK.
          */
        virtual int setRxBufferSize(uint8_t size)
        {
            return DEVICE_NOT_IMPLEMENTED;
        }

        /**
          * Reconfigures the size of our txBuff
          *
          * @param size the new size for our txBuff
          *
          * @return CODAL_SERIAL_IN_USE if another fiber is currently using this instance
          *         for transmission, otherwise DEVICE_OK.
          */
        virtual int setTxBufferSize(uint8_t size)
        {
            return DEVICE_NOT_IMPLEMENTED;
        }

        /**
          * The size of our rx buffer in bytes.
          *
          * @return the current size of rxBuff in bytes
          */
        virtual int getRxBufferSize()
        {
            return DEVICE_NOT_IMPLEMENTED;
        }

        /**
          * The size of our tx buffer in bytes.
          *
          * @return the current size of txBuff in bytes
          */
        virtual int getTxBufferSize()
        {
            return DEVICE_NOT_IMPLEMENTED;
        }

        /**
          * Sets the tail to match the head of our circular buffer for reception,
          * effectively clearing the reception buffer.
          *
          * @return CODAL_SERIAL_IN_USE if another fiber is currently using this instance
          *         for reception, otherwise DEVICE_OK.
          */
        virtual int clearRxBuffer()
        {
            return DEVICE_NOT_IMPLEMENTED;
        }

        /**
          * Sets the tail to match the head of our circular buffer for transmission,
          * effectively clearing the transmission buffer.
          *
          * @return CODAL_SERIAL_IN_USE if another fiber is currently using this instance
          *         for transmission, otherwise DEVICE_OK.
          */
        virtual int clearTxBuffer()
        {
            return DEVICE_NOT_IMPLEMENTED;
        }

        /**
          * The number of bytes currently stored in our rx buffer waiting to be digested,
          * by the user.
          *
          * @return The currently buffered number of bytes in our rxBuff.
          */
        virtual int rxBufferedSize()
        {
            return DEVICE_NOT_IMPLEMENTED;
        }

        /**
          * The number of bytes currently stored in our tx buffer waiting to be transmitted
          * by the hardware.
          *
          * @return The currently buffered number of bytes in our txBuff.
          */
        virtual int txBufferedSize()
        {
            return DEVICE_NOT_IMPLEMENTED;
        }

        /**
          * Determines if the serial bus is currently in use by another fiber for reception.
          *
          * @return The state of our mutex lock for reception.
          *
          * @note Only one fiber can call read at a time
          */
        virtual int rxInUse()
        {
            return DEVICE_NOT_IMPLEMENTED;
        }

        /**
          * Determines if the serial bus is currently in use by another fiber for transmission.
          *
          * @return The state of our mutex lock for transmition.
          *
          * @note Only one fiber can call send at a time
          */
        virtual int txInUse()
        {
            return DEVICE_NOT_IMPLEMENTED;
        }

        /**
          * Detaches a previously configured interrupt
          *
          * @param interruptType one of RxInterrupt or TxInterrupt
          *
          * @return DEVICE_OK on success.
          */
        virtual int detach(SerialInterruptType interruptType)
        {
            return DEVICE_NOT_IMPLEMENTED;
        }
    };
}

#endif

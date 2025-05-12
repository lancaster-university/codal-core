/*
The MIT License (MIT)

Copyright (c) 2016 Lancaster University.

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

#include "SerialStreamer.h"

using namespace codal;

/**
 * Creates a simple component that logs a stream of signed 16 bit data as signed 8-bit data over serial.
 * @param source a DataSource to measure the level of.
 * @param mode the format of the serialised data. Valid options are SERIAL_STREAM_MODE_BINARY (default), SERIAL_STREAM_MODE_DECIMAL, SERIAL_STREAM_MODE_HEX.
 * @param output the serial instance used to stream data to. Uses the first registered serial port by default.
 */
SerialStreamer::SerialStreamer(DataSource &source, int mode, Serial *output) : upstream(source)
{
    this->mode = mode;
    this->serial = output;

    // Register with our upstream component
    source.connect(*this);
}

/**
 * Define the serial port to use for diagnostics output
 */
void SerialStreamer::setSerial(Serial *serial)
{
    if (serial != NULL)
        this->serial = serial;
}

/**
 * Callback provided when data is ready.
 */
int SerialStreamer::pullRequest()
{
    static volatile int pr = 0;
     
    if(!pr)
    {
        pr++;
        while(pr)
        {
            lastBuffer = upstream.pull();
            streamBuffer(lastBuffer);
            pr--;
        }
    }
    else
    {
        pr++;
    }
    
    return DEVICE_OK;
}

/**
    * returns the last buffer processed by this component
    */
ManagedBuffer SerialStreamer::getLastBuffer()
{
    return lastBuffer;
}

/**
 * Callback provided when data is ready.
 */
void SerialStreamer::streamBuffer(ManagedBuffer buffer)
{
    // If we have no seriual port to stream to, then there's nothing to do...
    if (serial == NULL)
        return;

    int CRLF = 0;
    int bps = upstream.getFormat();

    // If a BINARY mode is requested, simply output all the bytes to the serial port.
    if( mode == SERIAL_STREAM_MODE_BINARY )
    {
        uint8_t *p = &buffer[0];
        uint8_t *end = p + buffer.length();

        while(p < end)
            serial->putc(*p++);
    }

    // if a HEX mode is requested, format using printf, framed by sample size..
    if( mode == SERIAL_STREAM_MODE_HEX || mode == SERIAL_STREAM_MODE_DECIMAL )
    {
        uint8_t *d = &buffer[0];
        uint8_t *end = d+buffer.length();
        uint32_t data;

        while(d < end)
        {
            data = *d++;

            if (bps > DATASTREAM_FORMAT_8BIT_SIGNED)
                data |= (*d++) << 8;
            if (bps > DATASTREAM_FORMAT_16BIT_SIGNED)
                data |= (*d++) << 16;
            if (bps > DATASTREAM_FORMAT_24BIT_SIGNED)
                data |= (*d++) << 24;

            if (mode == SERIAL_STREAM_MODE_HEX) {
                serial->printf("%x ", data);
            } else {
                // SERIAL_STREAM_MODE_DECIMAL
                if (bps == DATASTREAM_FORMAT_8BIT_SIGNED) {
                    serial->printf("%d ", (int8_t)(data & 0xFF));
                } else if (bps == DATASTREAM_FORMAT_16BIT_SIGNED) {
                    serial->printf("%d ", (int16_t)(data & 0xFFFF));
                } else if (bps == DATASTREAM_FORMAT_24BIT_SIGNED) {
                    // Move the sign bit to the most significant bit
                    int32_t signed_data = data & 0x7FFFFF;
                    if (data & (1 << 23)) {
                        signed_data |= 1 << 31;
                    }
                    serial->printf("%d ", signed_data);
                } else {
                    // Cannot print uint32_t correctly as serial.printf
                    // does not support "%u"
                    serial->printf("%d ", data);
                }
            }

            CRLF++;

            if (CRLF >= 16){
                serial->putc('\r');
                serial->putc('\n');
                CRLF = 0;
            }
        }

        if (CRLF > 0) {
            serial->putc( '\r' );
            serial->putc( '\n' );
        }
    }
}

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

#include "CodalDmesg.h"
#if DEVICE_DMESG_BUFFER_SIZE > 0

#include "CodalDevice.h"
#include "CodalConfig.h"
#include "CodalCompat.h"
#include "Timer.h"

CodalLogStore codalLogStore;
static void (*dmesg_flush_fn)(void) = NULL;

using namespace codal;

static void logwrite(const char *msg);

REAL_TIME_FUNC
static void logwriten(const char *msg, int l)
{
    target_disable_irq();

    if (codalLogStore.ptr + l >= sizeof(codalLogStore.buffer))
    {
        *(uint32_t *)codalLogStore.buffer = 0x0a2e2e2e; // "...\n"
        codalLogStore.ptr = 4;
        if (l >= (int)sizeof(codalLogStore.buffer) - 5)
        {
            msg = "DMESG line too long!\n";
            l = 21;
        }
    }

    char *dst = &codalLogStore.buffer[codalLogStore.ptr];
    int tmp = l;
    while (tmp--)
        *dst++ = *msg++;
    *dst = 0;
    codalLogStore.ptr += l;

    target_enable_irq();
}

static void logwrite(const char *msg)
{
    logwriten(msg, strlen(msg));
}

static void writeNum(char *buf, uint32_t n, bool full)
{
    int i = 0;
    int sh = 28;
    while (sh >= 0)
    {
        int d = (n >> sh) & 0xf;
        if (full || d || sh == 0 || i)
        {
            buf[i++] = d > 9 ? 'A' + d - 10 : '0' + d;
        }
        sh -= 4;
    }
    buf[i] = 0;
}

static void logwritenum(uint32_t n, bool full, bool hex)
{
    char buff[20];

    if (hex)
    {
        writeNum(buff, n, full);
        logwrite("0x");
    }
    else
    {
        itoa(n, buff);
    }

    logwrite(buff);
}

static void logwritedouble( double v, int precision = 8 )
{
    int iVal = (int)v;
    double fRem = v - (double)iVal;

    logwritenum( iVal, false, false );
    logwrite(".");

    while( precision-- > 0 ) {
        fRem = fRem * 10.0;
        logwritenum( (uint32_t)fRem, false, false );
        fRem -= (int)fRem;
    }
}

void codal_dmesg_nocrlf(const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    codal_vdmesg(format, false, arg);
    va_end(arg);
}

void codal_dmesg(const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    codal_vdmesg(format, true, arg);
    va_end(arg);
}

void codal_dmesg_with_flush(const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    codal_vdmesg(format, true, arg);
    va_end(arg);
    codal_dmesg_flush();
}

void codal_dmesg_set_flush_fn(void (*fn)(void))
{
    dmesg_flush_fn = fn;
}

void codal_dmesg_flush()
{
    if (dmesg_flush_fn)
        dmesg_flush_fn();
}

void codal_vdmesg(const char *format, bool crlf, va_list ap)
{
    const char *end = format;

    #if CONFIG_ENABLED(DMESG_SHOW_TIMES)
    logwritenum( (uint32_t)system_timer_current_time(), false, false );
    logwrite( "\t" );
    #endif

    #if CONFIG_ENABLED(DMESG_SHOW_FIBERS)
    logwritenum( (uint32_t)((uint64_t)currentFiber & 0x000000000000FFFF), false, true );
    logwrite( "\t" );
    #endif

    int param = 0;

    while (*end)
    {
        if (*end++ == '%')
        {
            logwriten(format, end - format - 1);
            param = 0;
l_parse_continue:
            switch (*end++)
            {
            case '0' ... '9': {
                int val = (int)*(end-1) - (int)'0';
                param = (param * 10) + val;
                goto l_parse_continue; // Note that labels are only valid within a single method context, so this is semi-safe
            } break;

            case 'c': {
                uint32_t val = va_arg(ap, uint32_t);
                logwriten((const char *)&val, 1);
            } break;
            case 'u': // should be printed as unsigned, but will do for now
            case 'd': {
                uint32_t val = va_arg(ap, uint32_t);
                logwritenum(val, false, false);
            } break;
            case 'x': {
                uint32_t val = va_arg(ap, uint32_t);
                logwritenum(val, false, true);
            } break;
            case 'p':
            case 'X': {
                uint32_t val = va_arg(ap, uint32_t);
                logwritenum(val, true, true);
            } break;
            case 's': {
                uint32_t val = va_arg(ap, uint32_t);
                logwrite((char *)(void *)val);
            } break;
            case 'f': {
                double val = va_arg(ap, double);
                if( param > 0 )
                    logwritedouble( val, param );
                else
                    logwritedouble( val, 4 );
            } break;
            case '%':
                logwrite("%");
                break;
            default:
                logwrite("???");
                break;
            }
            format = end;
        }
    }
    logwriten(format, end - format);

    if (crlf)
        logwrite("\r\n");
}

#endif

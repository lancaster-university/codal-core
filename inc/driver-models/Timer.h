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

#ifndef CODAL_TIMER_H
#define CODAL_TIMER_H

#include "CodalConfig.h"
#include "ErrorNo.h"

#ifndef CODAL_TIMER_DEFAULT_EVENT_LIST_SIZE
#define CODAL_TIMER_DEFAULT_EVENT_LIST_SIZE     10
#endif

namespace codal
{
    struct TimerEvent
    {
        uint16_t id;
        uint16_t value;
        CODAL_TIMESTAMP period;
        CODAL_TIMESTAMP timestamp;

        void set(CODAL_TIMESTAMP timestamp, CODAL_TIMESTAMP period, uint16_t id, uint16_t value)
        {
            this->timestamp = timestamp;
            this->period = period;
            this->id = id;
            this->value = value;
        }
    };

    class Timer
    {
    public:

        /**
          * Constructor for a generic system clock interface.
          */
        Timer();

        /**
          * Retrieves the current time tracked by this Timer instance
          * in milliseconds
          *
          * @return the timestamp in milliseconds
          */
        CODAL_TIMESTAMP getTime();

        /**
          * Retrieves the current time tracked by this Timer instance
          * in microseconds
          *
          * @return the timestamp in microseconds
          */
        CODAL_TIMESTAMP getTimeUs();

        /**
          * Configures this Timer instance to fire an event after period
          * milliseconds.
          *
          * @param period the period to wait until an event is triggered, in milliseconds.
          *
          * @param id the ID to be used in event generation.
          *
          * @param value the value to place into the Events' value field.
          */
        int eventAfter(CODAL_TIMESTAMP period, uint16_t id, uint16_t value);

        /**
          * Configures this Timer instance to fire an event after period
          * microseconds.
          *
          * @param period the period to wait until an event is triggered, in microseconds.
          *
          * @param id the ID to be used in event generation.
          *
          * @param value the value to place into the Events' value field.
          */
        int eventAfterUs(CODAL_TIMESTAMP period, uint16_t id, uint16_t value);

        /**
          * Configures this Timer instance to fire an event every period
          * milliseconds.
          *
          * @param period the period to wait until an event is triggered, in milliseconds.
          *
          * @param id the ID to be used in event generation.
          *
          * @param value the value to place into the Events' value field.
          */
        int eventEvery(CODAL_TIMESTAMP period, uint16_t id, uint16_t value);

        /**
          * Configures this Timer instance to fire an event every period
          * microseconds.
          *
          * @param period the period to wait until an event is triggered, in microseconds.
          *
          * @param id the ID to be used in event generation.
          *
          * @param value the value to place into the Events' value field.
          */
        int eventEveryUs(CODAL_TIMESTAMP period, uint16_t id, uint16_t value);

        /**
          * Cancels any events matching the given id and value.
          *
          * @param id the ID that was given upon a previous call to eventEvery / eventAfter
          *
          * @param value the value that was given upon a previous call to eventEvery / eventAfter
          */
        int cancel(uint16_t id, uint16_t value);

        /**
          * Destructor for this Timer instance
          */
        ~Timer();

        /**
         * Callback from physical timer implementation code.
         * @param t Indication that t time units (typically microsends) have elapsed.
         */
        void sync(CODAL_TIMESTAMP t);

        /**
         * Callback from physical timer implementation code, indicating a requested time span *may* have been completed.
         */
        void trigger();

        /**
         * request to the physical timer implementation code to provide a trigger callback at the given time.
         * note: it is perfectly legitimate for the implementation to trigger before this time if convenient.
         * @param t Indication that t time units (typically microsends) have elapsed.
         */
        virtual void triggerIn(CODAL_TIMESTAMP t) = 0;

        /**
         * request to the physical timer implementation code to trigger immediately.
         */
        virtual void syncRequest() = 0;

        /**
          * Enables interrupts for this timer instance.
          */
        virtual int enableInterrupts();

        /**
          * Disables interrupts for this timer instance.
          */
        virtual int disableInterrupts();

    protected:
        volatile CODAL_TIMESTAMP currentTime;
        volatile CODAL_TIMESTAMP currentTimeUs;
        volatile CODAL_TIMESTAMP overflow;

        TimerEvent *timerEventList;
        TimerEvent *nextTimerEvent;
        int eventListSize;

        TimerEvent *getTimerEvent();
        void releaseTimerEvent(TimerEvent *event);
        int setEvent(CODAL_TIMESTAMP period, uint16_t id, uint16_t value, bool repeat);


    };

    /*
     *
     * Convenience C API Interface that wraps this class, using the first compatible timer that is created
     *
     */

    /**
     * Determines the time since the device was powered on.
     *
     * @return the current time since power on in milliseconds
     */
    CODAL_TIMESTAMP system_timer_current_time();

    /**
     * Determines the time since the device was powered on.
     *
     * @return the current time since power on in microseconds
     */
    CODAL_TIMESTAMP system_timer_current_time_us();

    /**
     * Configure an event to occur every given number of microseconds.
     *
     * @param period the interval between events
     *
     * @param the value to fire against the current system_timer id.
     *
     * @return DEVICE_OK or DEVICE_NOT_SUPPORTED if no timer has been registered.
     */
    int system_timer_event_every_us(CODAL_TIMESTAMP period, uint16_t id, uint16_t value);

    /**
     * Configure an event to occur every given number of milliseconds.
     *
     * @param period the interval between events
     *
     * @param the value to fire against the current system_timer id.
     *
     * @return DEVICE_OK or DEVICE_NOT_SUPPORTED if no timer has been registered.
     */
    int system_timer_event_every(CODAL_TIMESTAMP period, uint16_t id, uint16_t value);

    /**
     * Configure an event to occur after a given number of microseconds.
     *
     * @param period the interval between events
     *
     * @param the value to fire against the current system_timer id.
     *
     * @return DEVICE_OK or DEVICE_NOT_SUPPORTED if no timer has been registered.
     */
    int system_timer_event_after(CODAL_TIMESTAMP period, uint16_t id, uint16_t value);

    /**
     * Configure an event to occur after a given number of milliseconds.
     *
     * @param period the interval between events
     *
     * @param the value to fire against the current system_timer id.
     *
     * @return DEVICE_OK or DEVICE_NOT_SUPPORTED if no timer has been registered.
     */
    int system_timer_event_after_us(CODAL_TIMESTAMP period, uint16_t id, uint16_t value);

    extern Timer* system_timer;
}

#endif

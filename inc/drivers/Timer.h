#ifndef CODAL_SYSTEM_CLOCK_H
#define CODAL_SYSTEM_CLOCK_H

#include "CodalConfig.h"
#include "ErrorNo.h"

#ifdef VS_DEBUG
#include "stdafx.h"
#include "malloc.h"
#include "string.h"
#include "stdio.h"

#define memclr(P,N) memset(P,0,N)
#define CODAL_TIMESTAMP uint32_t
#endif

#ifndef CODAL_TIMER_DEFAULT_EVENT_LIST_SIZE
#define CODAL_TIMER_DEFAULT_EVENT_LIST_SIZE     5
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
        virtual void triggerIn(CODAL_TIMESTAMP t) 
		{
#ifdef VS_DEBUG
			printf("Requested trigger in %d us\n", t);
#endif

		};

        /**
         * request to the physical timer implementation code to trigger immediately.
         */
        virtual void syncRequest() 
		{
#ifdef VS_DEBUG
			printf("SYNC requested...\n");
#endif
		};


    protected:
        CODAL_TIMESTAMP currentTime;
        CODAL_TIMESTAMP currentTimeUs;
        CODAL_TIMESTAMP overflow;

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

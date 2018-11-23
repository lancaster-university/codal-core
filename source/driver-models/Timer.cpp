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

/**
  * Definitions for the Device system timer.
  *
  * This module provides:
  *
  * 1) a concept of global system time since power up
  * 2) a simple periodic multiplexing API for the underlying mbed implementation.
  *
  * The latter is useful to avoid costs associated with multiple mbed Ticker instances
  * in codal components, as each incurs a significant additional RAM overhead (circa 80 bytes).
  */

#include "Timer.h"
#include "Event.h"
#include "CodalCompat.h"
#include "ErrorNo.h"
#include "codal_target_hal.h"
#include "CodalDmesg.h"

using namespace codal;

//
// Default system wide timer, if created.
//
Timer* codal::system_timer = NULL;

TimerEvent *Timer::getTimerEvent()
{
    // Find the first unused slot, and assign it.
    for (int i=0; i<eventListSize; i++)
    {
        if (timerEventList[i].id == 0)
            return &timerEventList[i];
    }

    // TODO: should try to realloc the list here.
    return NULL;
}

void Timer::releaseTimerEvent(TimerEvent *event)
{
    event->id = 0;
    if (nextTimerEvent == event)
        nextTimerEvent = NULL;
}

/**
 * Constructor for a generic system clock interface.
 */
Timer::Timer()
{
    // Register ourselves as the defualt timer - most recent timer wins.
    system_timer = this;

    // Create an empty event list of the default size.
    eventListSize = CODAL_TIMER_DEFAULT_EVENT_LIST_SIZE;
    timerEventList = (TimerEvent *) malloc(sizeof(TimerEvent) * CODAL_TIMER_DEFAULT_EVENT_LIST_SIZE);
    memclr(timerEventList, sizeof(TimerEvent) * CODAL_TIMER_DEFAULT_EVENT_LIST_SIZE);
    nextTimerEvent = NULL;

    // Reset clock
    currentTime = 0;
    currentTimeUs = 0;
}

/**
 * Retrieves the current time tracked by this Timer instance
 * in milliseconds
 *
 * @return the timestamp in milliseconds
 */
CODAL_TIMESTAMP Timer::getTime()
{
    syncRequest();
    return currentTime;
}

/**
 * Retrieves the current time tracked by this Timer instance
 * in microseconds
 *
 * @return the timestamp in microseconds
 */
CODAL_TIMESTAMP Timer::getTimeUs()
{
    syncRequest();
    return currentTimeUs;
}

int Timer::disableInterrupts()
{
    target_disable_irq();
    return DEVICE_OK;
}

int Timer::enableInterrupts()
{
    target_enable_irq();
    return DEVICE_OK;
}

int Timer::setEvent(CODAL_TIMESTAMP period, uint16_t id, uint16_t value, bool repeat)
{
    TimerEvent *evt = getTimerEvent();
    if (evt == NULL)
        return DEVICE_NO_RESOURCES;

    evt->set(getTimeUs() + period, repeat ? period: 0, id, value);

    disableInterrupts();

    if (nextTimerEvent == NULL || evt->timestamp < nextTimerEvent->timestamp)
    {
        nextTimerEvent = evt;
        triggerIn(period);
    }

    enableInterrupts();

    return DEVICE_OK;
}


/**
 * Cancels any events matching the given id and value.
 *
 * @param id the ID that was given upon a previous call to eventEvery / eventAfter
 *
 * @param value the value that was given upon a previous call to eventEvery / eventAfter
 */
int Timer::cancel(uint16_t id, uint16_t value)
{
    // Find the first unused slot, and assign it.
    for (int i=0; i<eventListSize; i++)
    {
        if (timerEventList[i].id == id && timerEventList[i].value == value)
        {
            timerEventList[i].id = 0;
            return DEVICE_OK;
        }
    }

    return DEVICE_INVALID_PARAMETER;

}

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
int Timer::eventAfter(CODAL_TIMESTAMP period, uint16_t id, uint16_t value)
{
    return eventAfterUs(period*1000, id, value);
}

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
int Timer::eventAfterUs(CODAL_TIMESTAMP period, uint16_t id, uint16_t value)
{
    return setEvent(period, id, value, false);
}

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
int Timer::eventEvery(CODAL_TIMESTAMP period, uint16_t id, uint16_t value)
{
    return eventEveryUs(period*1000, id, value);
}

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
int Timer::eventEveryUs(CODAL_TIMESTAMP period, uint16_t id, uint16_t value)
{
    return setEvent(period, id, value, true);
}

/**
 * Callback from physical timer implementation code.
 * @param t Indication that t time units (typically microsends) have elapsed.
 */
void Timer::sync(CODAL_TIMESTAMP t)
{
    // First, update our timestamps.
    currentTimeUs += t;
    overflow += t;

    while(overflow >= 1000)
    {
        overflow -= 1000;
        currentTime += 1;
    }
}

/**
 * Callback from physical timer implementation code.
 */
void Timer::trigger()
{
    int eventsFired;

    syncRequest();

    // Now, walk the list and trigger any events that are pending.
    do
    {
        eventsFired = 0;
        TimerEvent *e = timerEventList;

        for (int i=0; i<eventListSize; i++)
        {
            if (e->id != 0 && currentTimeUs >= e->timestamp)
            {
                // We need to trigger this event.
#if CONFIG_ENABLED(LIGHTWEIGHT_EVENTS)
                Event evt(e->id, e->value, currentTime);
#else
                Event evt(e->id, e->value, currentTimeUs);
#endif

                if (e->period == 0)
                    releaseTimerEvent(e);
                else
                    e->timestamp += e->period;

                // TODO: Handle rollover case above...
                eventsFired++;
            }
            e++;
        }

    } while (eventsFired);

    // always recompute nextTimerEvent - event firing could have added new timer events
    nextTimerEvent = NULL;

    TimerEvent *e = timerEventList;

    // Find the next most recent and schedule it.
    for (int i=0; i<eventListSize; i++)
    {
        if (e->id != 0 && (nextTimerEvent == NULL || (e->timestamp < nextTimerEvent->timestamp)))
            nextTimerEvent = e;
        e++;
    }

    if (nextTimerEvent) {
        // this may possibly happen if a new timer event was added to the queue while
        // we were running - it might be already in the past
        if (currentTimeUs < nextTimerEvent->timestamp)
            triggerIn(nextTimerEvent->timestamp - currentTimeUs);
        else
            triggerIn(1);
    }
}

/**
 * Destructor for this Timer instance
 */
Timer::~Timer()
{
}


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
CODAL_TIMESTAMP codal::system_timer_current_time()
{
    if(system_timer == NULL)
        return 0;

    return system_timer->getTime();
}

/**
  * Determines the time since the device was powered on.
  *
  * @return the current time since power on in microseconds
  */
CODAL_TIMESTAMP codal::system_timer_current_time_us()
{
    if(system_timer == NULL)
        return 0;

    return system_timer->getTimeUs();
}

/**
  * Configure an event to occur every period us.
  *
  * @param period the interval between events
  *
  * @param the value to fire against the current system_timer id.
  *
  * @return DEVICE_OK or DEVICE_NOT_SUPPORTED if no timer has been registered.
  */
int codal::system_timer_event_every_us(CODAL_TIMESTAMP period, uint16_t id, uint16_t value)
{
    if(system_timer == NULL)
        return DEVICE_NOT_SUPPORTED;

    return system_timer->eventEveryUs(period, id, value);
}

/**
  * Configure an event to occur after period us.
  *
  * @param period the interval between events
  *
  * @param the value to fire against the current system_timer id.
  *
  * @return DEVICE_OK or DEVICE_NOT_SUPPORTED if no timer has been registered.
  */
int codal::system_timer_event_after_us(CODAL_TIMESTAMP period, uint16_t id, uint16_t value)
{
    if(system_timer == NULL)
        return DEVICE_NOT_SUPPORTED;

    return system_timer->eventAfterUs(period, id, value);
}

/**
  * Configure an event to occur every period milliseconds.
  *
  * @param period the interval between events
  *
  * @param the value to fire against the current system_timer id.
  *
  * @return DEVICE_OK or DEVICE_NOT_SUPPORTED if no timer has been registered.
  */
int codal::system_timer_event_every(CODAL_TIMESTAMP period, uint16_t id, uint16_t value)
{
    if(system_timer == NULL)
        return DEVICE_NOT_SUPPORTED;

    return system_timer->eventEvery(period, id, value);
}

/**
  * Configure an event to occur after period millseconds.
  *
  * @param period the interval between events
  *
  * @param the value to fire against the current system_timer id.
  *
  * @return DEVICE_OK or DEVICE_NOT_SUPPORTED if no timer has been registered.
  */
int codal::system_timer_event_after(CODAL_TIMESTAMP period, uint16_t id, uint16_t value)
{
    if(system_timer == NULL)
        return DEVICE_NOT_SUPPORTED;

    return system_timer->eventAfter(period, id, value);
}

/**
 * Cancels any events matching the given id and value.
 *
 * @param id the ID that was given upon a previous call to eventEvery / eventAfter
 *
 * @param value the value that was given upon a previous call to eventEvery / eventAfter
 */
int codal::system_timer_cancel_event(uint16_t id, uint16_t value)
{
    if(system_timer == NULL)
        return DEVICE_NOT_SUPPORTED;

    return system_timer->cancel(id, value);
}

/**
 * Spin wait for a given number of microseconds.
 *
 * @param period the interval between events
 * @return DEVICE_OK or DEVICE_NOT_SUPPORTED if no timer has been registered.
 */
int codal::system_timer_wait_us(CODAL_TIMESTAMP period)
{
    if(system_timer == NULL)
        return DEVICE_NOT_SUPPORTED;

    CODAL_TIMESTAMP start = system_timer->getTimeUs();
    while(system_timer->getTimeUs() < start + period);

    return DEVICE_OK;
}

/**
 * Spin wait for a given number of milliseconds.
 *
 * @param period the interval between events
 * @return DEVICE_OK or DEVICE_NOT_SUPPORTED if no timer has been registered.
 */
int codal::system_timer_wait_ms(CODAL_TIMESTAMP period)
{
    return system_timer_wait_us(period * 1000);
}

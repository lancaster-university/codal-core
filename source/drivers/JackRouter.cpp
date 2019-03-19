/*
The MIT License (MIT)

Copyright (c) 2018 Lancaster University.

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

#include "JackRouter.h"
#include "ErrorNo.h"
#include "CodalDmesg.h"

#define MAX_IDLES 200

namespace codal
{

JackRouter::JackRouter(Pin &mid, Pin &sense, Pin &headphoneEnable, Pin &buzzerEnable,
                       Pin &powerEnable, JACDAC &jacdac)
    : mid(mid), sense(sense), hpEn(headphoneEnable), bzEn(buzzerEnable), pwrEn(powerEnable),
      jacdac(jacdac)
{
    status |= DEVICE_COMPONENT_STATUS_IDLE_TICK;

    floatUp = false;
    lastSenseFloat = false;
    numIdles = 0;
    numLows = 0;
    numSenseForced = 0;

    forcedState = state = JackState::None;
    setState(JackState::AllDown);
}

void JackRouter::forceState(JackState s)
{
    forcedState = s;
    if (s != JackState::None)
        setState(s);
}

void JackRouter::setState(JackState s)
{
    if (state == s)
        return;

    // shut down jacdac
    if (state == JackState::BuzzerAndSerial)
        jacdac.stop();

    state = s;
    hpEn.setDigitalValue(state == JackState::HeadPhones);
    bzEn.setDigitalValue(state == JackState::Buzzer || state == JackState::BuzzerAndSerial);
    pwrEn.setDigitalValue(state == JackState::BuzzerAndSerial);

    DMESG("Jack plug-in state: %d", state);

    // start jacdac
    if (state == JackState::BuzzerAndSerial)
        jacdac.start();

    Event(DEVICE_ID_JACKROUTER, (uint8_t)state);
}

void JackRouter::checkFloat()
{
    // pull the sense up or down and see if it sticks
    sense.setPull(floatUp ? PullMode::Up : PullMode::Down);
    sense.getDigitalValue(); // let it stabilize
    if (sense.getDigitalValue() != floatUp)
    {
        // if it didn't stick, it means sense is connected to something
        // ie., the jack was unplugged
        numSenseForced++;
    }
}

void JackRouter::idleCallback()
{
    if (forcedState != JackState::None)
        return;

    if (state == JackState::HeadPhones)
    {
        floatUp = !floatUp;
        checkFloat();
    }
    else if (state == JackState::Buzzer || state == JackState::AllDown)
    {
        // only do this once - this is a reliable test, unlike the one above
        if (numIdles == 0)
        {
            // in buzzer mode - try to turn on the power for a moment to see if sense is connected
            // to it
            pwrEn.setDigitalValue(1);
            floatUp = false;
            checkFloat();
            pwrEn.setDigitalValue(0);
        }
    }
    else if (state == JackState::BuzzerAndSerial)
    {
        if (numIdles == 0)
        {
            // power is already on, just see if sense is connected to it
            floatUp = false;
            checkFloat();
        }
    }

    if (state != JackState::BuzzerAndSerial)
    {
        if (!mid.getDigitalValue(PullMode::Up))
            numLows++;
    }

    numIdles++;

    if (numIdles < MAX_IDLES)
        return;

    // if (state != JackState::BuzzerAndSerial)
    //    DMESG("cycle: lf=%d nl=%d nf=%d", lastSenseFloat, numLows, numSenseForced);

    if (lastSenseFloat)
    {
        // something is connected - the sense was floating the previous cycle

        if (numLows == MAX_IDLES)
        {
            // the TX was low the entire time
            setState(JackState::HeadPhones);
        }
        else
        {
            // is the plug definetely in?
            if (lastSenseFloat > 5)
            {
                // the TX was up some time - assume networking mode
                setState(JackState::BuzzerAndSerial);
            }
        }
    }
    else
    {
        if (state == JackState::AllDown && numSenseForced == 0)
        {
            // something connected, will find out next cycle
        }
        else
        {
            // nothing connected - just send sound to the buzzer
            setState(JackState::Buzzer);
        }
    }

    if (numSenseForced == 0)
    {
        if (lastSenseFloat < 100)
            lastSenseFloat++;
    }
    else
        lastSenseFloat = 0;

    numIdles = 0;
    numLows = 0;
    numSenseForced = 0;
}

void JackRouter::logState()
{
    const char* jackRouterStateStr = "";

    switch (state)
    {
        case JackState::None:
            jackRouterStateStr = "None";
            break;
        case JackState::AllDown:
            jackRouterStateStr = "AllDown";
            break;
        case JackState::HeadPhones:
            jackRouterStateStr = "HeadPhones";
            break;
        case JackState::Buzzer:
            jackRouterStateStr = "Buzzer";
            break;
        case JackState::BuzzerAndSerial:
            jackRouterStateStr = "BuzzerAndSerial";
            break;
    }

    DMESG("Router state: %s", jackRouterStateStr);
}



} // namespace codal

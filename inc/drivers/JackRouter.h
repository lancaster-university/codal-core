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

#ifndef CODAL_JACK_ROUTER_H
#define CODAL_JACK_ROUTER_H

#include "CodalConfig.h"
#include "ErrorNo.h"
#include "Pin.h"
#include "PktSerial.h"
#include "DMASingleWireSerial.h"

namespace codal
{

enum class JackState : uint8_t
{
    AllDown = 1,
    HeadPhones,
    Buzzer,
    BuzzerAndSerial,
};

/**
 * Class definition for detector of plug/unplug audio jack events.
 */
class JackRouter : public CodalComponent
{
    bool floatUp;
    bool lastSenseFloat;
    uint8_t numIdles;
    uint8_t numLows;
    uint8_t numSenseForced;
    JackState state;

    Pin &mid;
    Pin &sense;
    Pin &hpEn;
    Pin &bzEn;
    Pin &pwrEn;

    PktSerial& serial;

    void setState(JackState s);
    /**
     * Check if the sense pin is floating
     */
    void checkFloat();

public:

    /**
     * Constructor.
     *
     * @param mid - pin connected directly to the middle ring of the jack
     * @param sense - pin connected to switch on the tip of the jack
     *        (when jack unplugged the switch is connected to tip)
     * @param headphoneEnable - when high, sound should be routed to middle ring and tip
     * @param buzzerEnable - when high, sound should be routed to the buzzer
     * @param powerEnable - when high, power should be routed to the tip of the jack
     */
    JackRouter(Pin &mid, Pin &sense, Pin &headphoneEnable, Pin &buzzerEnable, Pin &powerEnable, PktSerial &pkt);

    /**
     * Implement this function to receive a callback when the device is idling.
     */
    virtual void idleCallback();

    /**
     * Return the current state of the router.
     */
    JackState getState() { return state; }
};

} // namespace codal

#endif

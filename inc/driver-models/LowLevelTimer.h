#ifndef LOW_LEVEL_TIMER_H
#define LOW_LEVEL_TIMER_H

#include "CodalConfig.h"
#include "CodalComponent.h"

namespace codal
{

enum TimerMode
{
    TimerModeTimer = 0,
    TimerModeCounter
};

enum TimerBitMode
{
    BitMode8 = 0,
    BitMode16,
    BitMode24,
    BitMode32
};

class LowLevelTimer : public CodalComponent
{
    protected:
    TimerBitMode bitMode;
    uint8_t channel_count;

    public:

    void (*timer_pointer) (uint16_t channel_bitmsk);

    virtual int setIRQ(void (*timer_pointer) (uint16_t channel_bitmsk))
    {
        this->timer_pointer = timer_pointer;
        return DEVICE_OK;
    }

    LowLevelTimer(uint8_t channel_count)
    {
        this->channel_count = channel_count;
    }

    virtual int enable() = 0;

    virtual int enableIRQ() = 0;

    virtual int disable() = 0;

    virtual int disableIRQ() = 0;

    virtual int reset() = 0;

    virtual int setMode(TimerMode t) = 0;

    virtual int setCompare(uint8_t channel, uint32_t value)= 0;

    virtual int offsetCompare(uint8_t channel, uint32_t value) = 0;

    virtual int clearCompare(uint8_t channel);

    virtual uint32_t captureCounter() = 0;

    virtual int setClockSpeed(uint32_t speedKHz);

    virtual int setBitMode(TimerBitMode t) = 0;

    virtual TimerBitMode getBitMode()
    {
        return bitMode;
    }

    int getChannelCount()
    {
        return channel_count;
    }
};
}


#endif
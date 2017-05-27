#ifndef CODAL_TARGET_HAL_H
#define CODAL_TARGET_HAL_H

extern "C"
{
    void target_enable_irq();

    void target_disable_irq();

    void target_reset();

    void target_wait(unsigned long milliseconds);

    void target_wait_for_event();

    void target_panic(int statusCode);
}


#endif

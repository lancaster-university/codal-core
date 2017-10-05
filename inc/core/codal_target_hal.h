#ifndef CODAL_TARGET_HAL_H
#define CODAL_TARGET_HAL_H

#include "platform_includes.h"

extern "C"
{
    void target_enable_irq();

    void target_disable_irq();

    void target_reset();

    void target_wait(unsigned long milliseconds);

    void target_wait_for_event();

    void target_panic(int statusCode);

    PROCESSOR_WORD_TYPE fiber_initial_stack_base();
    /**
      * Configures the link register of the given tcb to have the value function.
      *
      * @param tcb The tcb to modify
      * @param function the function the link register should point to.
      */
    void tcb_configure_lr(void* tcb, PROCESSOR_WORD_TYPE function);

    void* tcb_allocate();

    /**
      * Configures the link register of the given tcb to have the value function.
      *
      * @param tcb The tcb to modify
      * @param function the function the link register should point to.
      */
    void tcb_configure_sp(void* tcb, PROCESSOR_WORD_TYPE sp);

    void tcb_configure_stack_base(void* tcb, PROCESSOR_WORD_TYPE stack_base);

    PROCESSOR_WORD_TYPE tcb_get_stack_base(void* tcb);

    PROCESSOR_WORD_TYPE get_current_sp();

    PROCESSOR_WORD_TYPE tcb_get_sp(void* tcb);

    void tcb_configure_args(void* tcb, PROCESSOR_WORD_TYPE ep, PROCESSOR_WORD_TYPE cp, PROCESSOR_WORD_TYPE pm);
}


#endif

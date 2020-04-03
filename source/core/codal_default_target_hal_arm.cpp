#include "codal_target_hal.h"
#include "CodalDmesg.h"
#include "CodalCompat.h"

#ifdef __arm__

#define WEAK __attribute__((weak))

static int8_t irq_disabled;
WEAK void target_enable_irq()
{
    irq_disabled--;
    if (irq_disabled <= 0)
    {
        irq_disabled = 0;
        asm volatile("cpsie i" : : : "memory");
    }
}

WEAK void target_disable_irq()
{
    asm volatile("cpsid i" : : : "memory");
    irq_disabled++;
}

WEAK void target_wait_for_event()
{
    asm volatile("wfe");
}

/**
 *  Thread Context for an ARM Cortex M0 core.
 *
 * This is probably overkill, but the ARMCC compiler uses a lot register optimisation
 * in its calling conventions, so better safe than sorry!
 */
struct PROCESSOR_TCB
{
    uint32_t R0;
    uint32_t R1;
    uint32_t R2;
    uint32_t R3;
    uint32_t R4;
    uint32_t R5;
    uint32_t R6;
    uint32_t R7;
    uint32_t R8;
    uint32_t R9;
    uint32_t R10;
    uint32_t R11;
    uint32_t R12;
    uint32_t SP;
    uint32_t LR;
    uint32_t stack_base;
};

WEAK PROCESSOR_WORD_TYPE fiber_initial_stack_base()
{
    return DEVICE_STACK_BASE;
}

WEAK void *tcb_allocate()
{
    return (void *)malloc(sizeof(PROCESSOR_TCB));
}

/**
 * Configures the link register of the given tcb to have the value function.
 *
 * @param tcb The tcb to modify
 * @param function the function the link register should point to.
 */
WEAK void tcb_configure_lr(void *tcb, PROCESSOR_WORD_TYPE function)
{
    PROCESSOR_TCB *tcbPointer = (PROCESSOR_TCB *)tcb;
    tcbPointer->LR = function;
}

/**
 * Configures the link register of the given tcb to have the value function.
 *
 * @param tcb The tcb to modify
 * @param function the function the link register should point to.
 */
WEAK void tcb_configure_sp(void *tcb, PROCESSOR_WORD_TYPE sp)
{
    PROCESSOR_TCB *tcbPointer = (PROCESSOR_TCB *)tcb;
    tcbPointer->SP = sp;
}

WEAK void tcb_configure_stack_base(void *tcb, PROCESSOR_WORD_TYPE stack_base)
{
    PROCESSOR_TCB *tcbPointer = (PROCESSOR_TCB *)tcb;
    tcbPointer->stack_base = stack_base;
}

WEAK PROCESSOR_WORD_TYPE tcb_get_stack_base(void *tcb)
{
    PROCESSOR_TCB *tcbPointer = (PROCESSOR_TCB *)tcb;
    return tcbPointer->stack_base;
}

WEAK PROCESSOR_WORD_TYPE get_current_sp()
{
    register uint32_t result;
    asm volatile("MRS %0, msp" : "=r"(result));
    return (result);
}

WEAK PROCESSOR_WORD_TYPE tcb_get_sp(void *tcb)
{
    PROCESSOR_TCB *tcbPointer = (PROCESSOR_TCB *)tcb;
    return tcbPointer->SP;
}

WEAK void tcb_configure_args(void *tcb, PROCESSOR_WORD_TYPE ep, PROCESSOR_WORD_TYPE cp,
                             PROCESSOR_WORD_TYPE pm)
{
    PROCESSOR_TCB *tcbPointer = (PROCESSOR_TCB *)tcb;
    tcbPointer->R0 = (uint32_t)ep;
    tcbPointer->R1 = (uint32_t)cp;
    tcbPointer->R2 = (uint32_t)pm;
}

extern PROCESSOR_WORD_TYPE __end__;
WEAK PROCESSOR_WORD_TYPE target_heap_start()
{
    return (PROCESSOR_WORD_TYPE)(&__end__);
}
#endif
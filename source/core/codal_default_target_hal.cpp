#include "codal_target_hal.h"
#include "CodalDmesg.h"
#include "CodalCompat.h"
#include "Timer.h"

__attribute__((weak)) void target_wait(uint32_t milliseconds)
{
    codal::system_timer_wait_ms(milliseconds);
}

REAL_TIME_FUNC
__attribute__((weak)) void target_wait_us(uint32_t us)
{
    codal::system_timer_wait_us(us);
}

__attribute__((weak)) int target_seed_random(uint32_t rand)
{
    return codal::seed_random(rand);
}

__attribute__((weak)) int target_random(int max)
{
    return codal::random(max);
}

__attribute__((weak)) void target_panic(int statusCode)
{
    target_disable_irq();

    DMESG("*** CODAL PANIC : [%d]", statusCode);
    while (1)
    {
    }
}

__attribute__((weak)) void target_scheduler_idle()
{
    // if not implemented, default to WFI
    target_wait_for_event();
}

__attribute__((weak)) void target_deepsleep()
{
    // if not implemented, default to WFI
    target_wait_for_event();
}

// Preprocessor Directive to ignore redecleration when using clang
#ifndef __clang__
	/**
	 * Default implementation of atomic fetch and add opertaion.
	 * GCC provides this where possible, but this is not supported on some CPU architectures...
	 *
	 * @param ptr pointer to the memory to access.
	 * @param value the value to add to the memory location.
	 * @return the value of th ememory location BEFORE the add operation took place.
	 */
	__attribute__((weak)) short unsigned int __sync_fetch_and_add_2 (volatile void *ptr, short unsigned int value)
	{

	#if CONFIG_ENABLED(DISABLE_IRQ_FOR_SOFTWARE_ATOMICS)
		target_disable_irq();
	#endif

		uint16_t *p = (uint16_t *)ptr;
		uint16_t old = *p;
		*p += value;

	#if CONFIG_ENABLED(DISABLE_IRQ_FOR_SOFTWARE_ATOMICS)
		target_enable_irq();
	#endif

		return old;
	}
#endif

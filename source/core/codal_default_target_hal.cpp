#include "codal_target_hal.h"
#include "CodalDmesg.h"
#include "CodalCompat.h"
#include "Timer.h"

__attribute__((weak)) void target_wait(uint32_t milliseconds)
{
    codal::system_timer_wait_ms(milliseconds);
}

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

__attribute__((weak)) void target_deepsleep()
{
    // if not implemented, default to WFI
    target_wait_for_event();
}

static uint32_t murmur_hash2_core(uint32_t *d, int words)
{
    const uint32_t m = 0x5bd1e995;
    uint32_t h = 0;

    while (words--)
    {
        uint32_t k = *d++;
        k *= m;
        k ^= k >> 24;
        k *= m;
        h *= m;
        h ^= k;
    }

    return h;
}

// https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
static uint32_t hash_fnv1a(const void *data, unsigned len)
{
    const uint8_t *d = (const uint8_t *)data;
    uint32_t h = 0x811c9dc5;
    while (len--)
        h = (h ^ *d++) * 0x1000193;
    return h;
}

/**
 * Compute 64-bit hash of manufacturer provided serial number.
 */
__attribute__((weak)) uint64_t target_hash_serial_number(uint32_t *serialdata, unsigned numwords)
{
    uint32_t w1 = hash_fnv1a(serialdata, numwords << 2);
    uint32_t w0 = murmur_hash2_core((uint32_t *)serialdata, numwords);
    w0 &= ~0x02000000; // clear "universal" bit
    return ((uint64_t)w0 << 32 | w1);
}

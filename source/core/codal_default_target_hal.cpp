#include "codal_target_hal.h"
#include "CodalDmesg.h"
#include "CodalCompat.h"
#include "Timer.h"

#define WEAK __attribute__((weak))

WEAK void target_wait(uint32_t milliseconds)
{
    codal::system_timer_wait_ms(milliseconds);
}

WEAK void target_wait_us(uint32_t us)
{
    codal::system_timer_wait_us(us);
}

WEAK int target_seed_random(uint32_t rand)
{
    return codal::seed_random(rand);
}

WEAK int target_random(int max)
{
    return codal::random(max);
}

WEAK void target_panic(int statusCode)
{
    target_disable_irq();

    DMESG("*** CODAL PANIC : [%d]", statusCode);
    while (1)
    {
    }
}

WEAK void target_deepsleep()
{
    // if not implemented, default to WFI
    target_wait_for_event();
}

static uint32_t murmur_hash2_core(const uint32_t *d, int words)
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
WEAK uint64_t target_get_serial()
{
    static uint64_t cache;
    if (!cache)
    {
        uint32_t serialdata[8];
        unsigned numbytes = target_get_serial_buffer(serialdata, sizeof(serialdata));
        if (numbytes > sizeof(serialdata))
            target_panic(DEVICE_HARDWARE_CONFIGURATION_ERROR);
        uint32_t w1 = hash_fnv1a(serialdata, numbytes);
        uint32_t w0 = murmur_hash2_core(serialdata, numbytes >> 2);
        w0 &= ~0x02000000; // clear "universal" bit
        cache = ((uint64_t)w0 << 32 | w1);
    }
    return cache;
}

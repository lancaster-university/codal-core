#include "SNORFS.h"

#include <assert.h>
#include <stdio.h>

#define oops() assert(false)

class MemFlash : public codal::SPIFlash
{
    uint32_t npages;
    uint8_t *data;
    int erase(uint32_t addr, uint32_t len)
    {
        assert(addr % len == 0);
        assert(addr + len <= chipSize());
        for (uint32_t i = 0; i < len; ++i)
            data[addr + i] = 0xff;
        return 0;
    }
    uint32_t chipSize() { return npages * SPIFLASH_PAGE_SIZE; }

public:
    MemFlash(int npages)
    {
        this->npages = npages;
        data = new uint8_t[chipSize()];
    }

    int numPages() { return npages; }
    int readBytes(uint32_t addr, void *buffer, uint32_t len)
    {
        assert(addr + len <= chipSize());
        assert(len <= SPIFLASH_PAGE_SIZE); // needed?
        memcpy(buffer, data + addr, len);
        return 0;
    }
    int writeBytes(uint32_t addr, const void *buffer, uint32_t len)
    {
        assert(len <= SPIFLASH_PAGE_SIZE);
        assert(addr / SPIFLASH_PAGE_SIZE == (addr + len - 1) / SPIFLASH_PAGE_SIZE);
        assert(addr + len <= chipSize());
        uint8_t *ptr = (uint8_t *)buffer;
        for (uint32_t i = 0; i < len; ++i)
        {
            assert(data[addr + i] == 0xff || *ptr == 0x00);
            data[addr + i] = *ptr++;
        }
        return 0;
    }
    int eraseSmallRow(uint32_t addr) { return erase(addr, SPIFLASH_SMALL_ROW_SIZE); }
    int eraseBigRow(uint32_t addr) { return erase(addr, SPIFLASH_BIG_ROW_SIZE); }
    int eraseChip() { return erase(0, chipSize()); }
};

int main()
{
    MemFlash flash(1024 * 1024 / SPIFLASH_PAGE_SIZE);
    flash.eraseChip();
    codal::snorfs::FS fs(flash);

    fs.debugDump();
    printf("OK\n");

    return 0;
}
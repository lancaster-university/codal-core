#ifndef CODAL_SPIFLASH_H
#define CODAL_SPIFLASH_H

// this seems common to many different SPI flash parts
// they sometimes also have medium row of 32k
#define SPIFLASH_PAGE_SIZE 256
#define SPIFLASH_SMALL_ROW_PAGES 16
#define SPIFLASH_BIG_ROW_PAGES 256
#define SPIFLASH_SMALL_ROW_SIZE (SPIFLASH_SMALL_ROW_PAGES * SPIFLASH_PAGE_SIZE) // 4k
#define SPIFLASH_BIG_ROW_SIZE (SPIFLASH_BIG_ROW_PAGES * SPIFLASH_PAGE_SIZE)     // 64k

namespace codal
{
class SPIFlash
{
public:
    virtual int numPages() = 0;
    virtual int readBytes(uint32_t addr, void *buffer, uint32_t len) = 0;
    // len <= SPIFLASH_PAGE_SIZE; block cannot span pages
    virtual int writeBytes(uint32_t addr, const void *buffer, uint32_t len) = 0;
    virtual int eraseSmallRow(uint32_t addr) = 0;
    virtual int eraseBigRow(uint32_t addr) = 0;
    virtual int eraseChip() = 0;
};
}

#endif

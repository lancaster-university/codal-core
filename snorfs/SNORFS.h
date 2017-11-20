#ifndef CODAL_SNORFS_H
#define CODAL_SNORFS_H

#ifndef __arm__
#include "SNORFS-test.h"
#endif

#include "SPIFlash.h"

// each meta row gives you 256 more file entries
#define SNORFS_META_ROWS 2

#define SNORFS_RESERVED_META_PAGES (SPIFLASH_SMALL_ROW_PAGES)

namespace codal
{
namespace snorfs
{

class File;

// Supported flash size: 1-16MB
class FS
{
    friend class File;

    SPIFlash &flash;
    uint8_t buf[SPIFLASH_PAGE_SIZE];

    uint8_t *rowRemapCache;
    uint8_t numDataRows;
    uint8_t metaFree[SNORFS_META_ROWS];

    void mount();
    int firstFree(uint32_t addr, int startOff);
    void oops() { abort(); }

    uint32_t dataRowAddr(uint8_t rowIdx) { return rowRemapCache[rowIdx] * SPIFLASH_BIG_ROW_SIZE; }
    uint32_t dataIndexAddr(uint16_t ptr) { return dataRowAddr(ptr >> 8) + (ptr & 0xff); }
    uint32_t dataNextPtrAddr(uint16_t ptr)
    {
        return dataRowAddr(ptr >> 8) + SPIFLASH_BIG_ROW_SIZE - 2 * SPIFLASH_PAGE_SIZE +
               2 * (ptr & 0xff);
    }
    uint32_t dataDataAddr(uint16_t ptr)
    {
        return dataRowAddr(ptr >> 8) + SPIFLASH_PAGE_SIZE * (ptr & 0xff);
    }

    // last page in meta row is index
    uint32_t metaIdxAddr(int i) { return (i + 1) * SPIFLASH_BIG_ROW_SIZE - SPIFLASH_PAGE_SIZE; }
    uint32_t metaPageAddr(int i, int j)
    {
        return i * SPIFLASH_BIG_ROW_SIZE + j * SPIFLASH_PAGE_SIZE;
    }
    uint32_t getRemapSize()
    {
        return min(SPIFLASH_SMALL_ROW_SIZE / numDataRows, SPIFLASH_PAGE_SIZE);
    }
    uint16_t findFreeDataPage(int startRow);

public:
    FS(SPIFlash &f);
    ~FS();
};

class File
{
    FS &fs;
    uint32_t metaSize;
    uint32_t currSeekOffset;
    uint16_t firstPage; // row idx : page idx
    uint16_t currSeekPage;
    uint8_t metaRow;
    uint8_t metaPage;
    uint8_t metaSizeOff;

    uint32_t metaPageAddr() { return fs.metaPageAddr(metaRow, metaPage); }
    void oops() { fs.oops(); }

    void updateSize(uint32_t newSize);
    void readSize();
    void findFirstPage();
    void rewind();
    void seekNextPage();
    void allocatePage();

public:
    File(FS &f, const char *filename);
    void seek(uint32_t pos);
    int read(uint8_t *data, uint32_t len);
    void append(const uint8_t *data, uint32_t len);
};

}
}

#endif

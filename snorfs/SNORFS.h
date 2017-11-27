#ifndef CODAL_SNORFS_H
#define CODAL_SNORFS_H

#include "SPIFlash.h"

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
    uint8_t numRows;
    uint8_t numMetaRows;
    uint8_t freeRow;

    uint32_t randomSeed;

    // this is for data pages only
    uint16_t fullPages;
    uint16_t deletedPages;
    uint16_t freePages;


    uint32_t rowAddr(uint8_t rowIdx)
    {
        if (rowIdx >= numRows)
            oops();
        return rowRemapCache[rowIdx] * SPIFLASH_BIG_ROW_SIZE;
    }
    uint32_t indexAddr(uint16_t ptr) { return rowAddr(ptr >> 8) + (ptr & 0xff); }
    uint32_t nextPtrAddr(uint16_t ptr)
    {
        return rowAddr(ptr >> 8) + SPIFLASH_BIG_ROW_SIZE - 2 * SPIFLASH_PAGE_SIZE +
               2 * (ptr & 0xff);
    }
    uint32_t pageAddr(uint16_t ptr)
    {
        return rowAddr(ptr >> 8) + SPIFLASH_PAGE_SIZE * (ptr & 0xff);
    }

    int firstFree(uint16_t pageIdx);
    uint16_t findFreePage(bool isData);
    uint32_t random(uint32_t max);
    void feedRandom(uint32_t max);
    void mount();
    void format();
    uint16_t findMetaEntry(const char *filename);
    uint16_t createMetaPage(const char *filename);

public:
    FS(SPIFlash &f);
    ~FS();
    void debugDump();
    // returns NULL if file doesn't exists and create==false
    File *open(const char *filename, bool create = true);
    void progress();
};

class File
{
    // Invariants:
    // firstPage == 0 <==> no pages has been allocated
    // readOffset % SPIFLASH_PAGE_SIZE == 0 && readPage != 0 ==>
    //       readPage is on page for (readOffset - 1)
    // writePage % SPIFLASH_PAGE_SIZE == 0 && writePage != 0 ==>
    //       writePage is on page for (metaSize - 1)
    // if readPage is 0 it needs to be recomputed
    // if writePage is 0 it needs to be recomputed

    friend class FS;

    FS &fs;
    uint32_t metaSize;
    uint16_t firstPage; // row idx : page ID
    // this is for reading
    uint16_t readPage; // row idx : page idx
    uint32_t readOffset;
    // this is for writing (append)
    uint16_t writePage;
    uint16_t metaPage;
    uint8_t metaSizeOff;

    uint32_t metaPageAddr() { return fs.pageAddr(metaPage); }

    void readSize();
    void findFirstPage();
    void rewind();
    void seekNextPage();
    void allocatePage();
    void newMetaPage();
    void findFreeMetaPage();
    void computeWritePage();
    void saveSizeDiff(int32_t sizeDiff);
    void truncateCore();
    void appendCore(const void *data, uint32_t len);
    File(FS &f, uint16_t filePage);
    File(FS &f, const char *filename);

public:
    int read(void *data, uint32_t len);
    void append(const void *data, uint32_t len);
    void seek(uint32_t pos);
    uint32_t size() { return metaSize; }
    uint32_t tell() { return readOffset; }
    uint32_t fileID() { return metaPage; }
    void debugDump();
    bool isDeleted() { return writePage == 0xffff; }
    void overwrite(const void *data, uint32_t len);
    void del();
    void truncate() { overwrite(NULL, 0); }
};
}
}

#endif

#include "SNORFS.h"

/*
TODO:
- keep files in list in FS
- enforce only one handle per file
- nuke caches in files on GC
- implement file data GC
- implement meta-data GC
*/

using namespace codal::snorfs;

#define SNORFS_RESERVED_META_PAGES (SPIFLASH_SMALL_ROW_PAGES)
#define SNORFS_REP_START_BLOCK 32
#define SNORFS_END_SIZE (SPIFLASH_PAGE_SIZE - SNORFS_REP_START_BLOCK * 2)

static uint8_t fnhash(const char *fn)
{
    uint32_t h = 0x811c9dc5;
    while (*fn)
        h = (h * 0x1000193) ^ (uint8_t)*fn++;
    h &= 0xff;
    if (h == 0x00)
        return 0x01;
    if (h == 0xff)
        return 0x02;
    return h;
}

FS::FS(SPIFlash &f) : flash(f)
{
    numRows = 0;
}

int FS::firstFree(uint32_t addr)
{
    flash.readBytes(addr, buf, SPIFLASH_PAGE_SIZE);
    uint8_t firstFree = 0;
    for (int k = 1; k < SPIFLASH_PAGE_SIZE - 2; ++k)
    {
        if (firstFree == 0)
        {
            if (buf[k] == 0xff)
                firstFree = k;
        }
        else
        {
            if (buf[k] != 0xff)
                oops();
        }
    }
    return firstFree;
}

void FS::progress()
{
    // blink LED or something
}

void FS::format()
{
    uint32_t end = flash.numPages() * SPIFLASH_PAGE_SIZE;
    uint8_t rowIdx = 0;
    const char *header = "SNORFS\x02\x00";

    for (uint32_t addr = 0; addr < end; addr += SPIFLASH_BIG_ROW_SIZE)
    {
        progress();
        for (uint32_t off = 0; off < SPIFLASH_BIG_ROW_SIZE; off += SPIFLASH_PAGE_SIZE)
        {
            flash.readBytes(addr + off, buf, SPIFLASH_PAGE_SIZE);
            for (int i = 0; i < SPIFLASH_PAGE_SIZE; ++i)
                if (buf[i] != 0xff)
                    goto clearRow;
        }
        goto flashMeta;
    clearRow:
        progress();
        flash.eraseBigRow(addr);
    flashMeta:
        // the last empty row?
        if (addr + SPIFLASH_BIG_ROW_SIZE >= end)
            rowIdx = 0xff;
        LOG("format: %d\n", rowIdx);
        flash.writeBytes(addr, &rowIdx, 1);
        if (rowIdx < 4)
            flash.writeBytes(addr + SPIFLASH_PAGE_SIZE - 2, header + rowIdx * 2, 2);
        rowIdx++;
    }
    progress();
}

void FS::mount()
{
    if (numRows > 0)
        return;

    numRows = (flash.numPages() / SPIFLASH_BIG_ROW_PAGES) - 1;
    rowRemapCache = new uint8_t[numRows];

    for (;;)
    {
        memset(rowRemapCache, 0xff, numRows);

        uint8_t fsheader[8];
        int freeRow = -1;
        for (unsigned i = 0; i < numRows + 1; ++i)
        {
            auto addr = i * SPIFLASH_BIG_ROW_SIZE;
            if (i * 2 < sizeof(fsheader))
                flash.readBytes(addr + SPIFLASH_PAGE_SIZE - 2, fsheader + i * 2, 2);
            uint8_t remap;
            flash.readBytes(addr, &remap, 1);
            if (remap == 0xff)
            {
                if (freeRow == -1)
                    freeRow = remap;
                else
                    goto fmt;
            }
            else
            {
                if (remap >= numRows || rowRemapCache[remap] != 0xff)
                    goto fmt;
                rowRemapCache[remap] = i;
            }
        }

        numMetaRows = fsheader[6];
        this->freeRow = freeRow;

        if (freeRow == -1 || !numMetaRows || numMetaRows > numRows / 2)
            goto fmt;

        if (memcmp(fsheader, "SNORFS", 6) == 0)
            break;

    fmt:
        format();
        // and re-try the mount
    }
}

uint16_t FS::findFreeDataPage(int startRow)
{
    bool wrapped = false;
    int ptr = max((int)numMetaRows, startRow);
    for (;;)
    {
        int fr = firstFree(indexAddr(ptr << 8));
        if (fr)
            return (ptr << 8) | fr;
        ptr++;
        if (ptr == numRows)
        {
            if (wrapped)
                return 0;
            ptr = numMetaRows;
            wrapped = true;
        }
    }
}

FS::~FS()
{
    delete rowRemapCache;
}

uint16_t FS::findFreeMetaPage()
{
    for (int ptr = 0; ptr < numMetaRows; ++ptr)
    {
        int fr = firstFree(indexAddr(ptr << 8));
        if (fr)
            return (ptr << 8) | fr;
    }
    oops(); // out of meta space
    return 0;
}

void File::newMetaPage()
{
    uint32_t prevAddr = metaPageAddr();
    uint32_t prevIdx = fs.indexAddr(metaPage);
    uint32_t prevSize = metaSize;

    fs.flash.readBytes(prevAddr, fs.buf, 64);
    fs.buf[64] = 0;
    const char *fn = (const char *)fs.buf + 1;
    int fnlen = strlen(fn);
    uint8_t h = fnhash(fn);
    memset(fs.buf + fnlen + 2, 0xff, SPIFLASH_PAGE_SIZE - (fnlen + 2));
    readSize();
    if (metaSize != 0)
        oops();

    metaPage = fs.findFreeMetaPage();

    fs.flash.readBytes(prevAddr, fs.buf, fnlen + 2);
    fs.flash.writeBytes(metaPageAddr(), fs.buf, fnlen + 2);
    metaSize = prevSize;
    saveSizeDiff(metaSize);
    fs.flash.writeBytes(metaPageAddr() + SNORFS_END_SIZE, &firstPage, 2);

    uint8_t zero = 0;
    fs.flash.writeBytes(prevAddr, &zero, 1);
    fs.flash.writeBytes(prevIdx, &zero, 1);
    fs.flash.writeBytes(fs.indexAddr(metaPage), &h, 1);
}

void File::saveSizeDiff(int32_t sizeDiff)
{
    if (!sizeDiff)
        return;
    uint8_t buf[4];
    int num = 0;
    if (1 <= sizeDiff && sizeDiff <= 127)
    {
        buf[num++] = sizeDiff;
    }
    else
    {
        int b0 = sizeDiff >> 13;
        if (b0 != -1 && b0 != 0)
            buf[num++] = 0x80 | (b0 >> 1);
        buf[num++] = 0x80 | (sizeDiff >> 7);
        buf[num++] = sizeDiff & 0x7f;
    }
    for (int i = 0; i < num; ++i)
        buf[i] ^= 0xff;
    if (metaSizeOff + num >= SNORFS_END_SIZE)
    {
        newMetaPage(); // this will call back here, but only once
        return;
    }
    LOG("WRSZ: num=%d at %d: 0x%x 0x%x, meta=%x\n", num, metaSizeOff, buf[0] ^ 0xff, buf[1] ^ 0xff,
        metaPageAddr());
    fs.flash.writeBytes(metaPageAddr() + metaSizeOff, buf, num);
    metaSizeOff += num;
}

void File::readSize()
{
    uint8_t *buf = fs.buf;
    int len = SNORFS_END_SIZE;
    while (len > 0 && *buf)
    {
        len--;
        buf++;
    }
    if (len == 0)
        oops();

    buf++;
    len--; // skip NUL at the end of filename

    uint32_t res = 0;
    int32_t tmp = 0;
    bool mid = false;
    while (len--)
    {
        uint8_t c = 0xff ^ *buf++;
        if (!c & !mid)
            break;

        if (c & 0x80)
        {

            if (!mid)
            {
                mid = true;
                tmp = ((int32_t)c << (32 - 7)) >> (32 - 14);
            }
            else
            {
                tmp = (tmp | (c & 0x7f)) << 7;
            }
        }
        else
        {
            res += tmp | c;
            mid = false;
            tmp = 0;
        }
    }

    metaSizeOff = buf - fs.buf - 1;
    metaSize = res;
}

void File::findFirstPage()
{
    firstPage = 0;
    for (int i = 0; i < SNORFS_REP_START_BLOCK; ++i)
    {
        auto tmp = *(uint16_t *)&fs.buf[SNORFS_END_SIZE + i * 2];
        if (tmp == 0xffff)
            break;
        firstPage = tmp;
    }
}

void File::rewind()
{
    readPage = 0;
    readOffset = 0;
}

File *FS::open(const char *filename, bool create)
{
    auto page = findMetaEntry(filename);
    if (page == 0)
    {
        if (create)
            page = createMetaPage(filename);
        else
            return NULL;
    }
    return new File(*this, page);
}

uint16_t FS::findMetaEntry(const char *filename)
{
    mount();

    uint8_t h = fnhash(filename);
    uint16_t buflen = strlen(filename) + 2;

    if (buflen > 64)
        oops();

    for (int i = 0; i < numMetaRows; ++i)
    {
        flash.readBytes(indexAddr(i << 8), buf, SPIFLASH_PAGE_SIZE);
        for (int j = 1; j < SPIFLASH_PAGE_SIZE - 2; ++j)
        {
            if (buf[j] == h)
            {
                uint8_t tmp[buflen];
                uint16_t pageIdx = (i << 8) | j;
                auto addr = pageAddr(pageIdx);
                flash.readBytes(addr, tmp, buflen);
                if (tmp[0] != 0 && memcmp(tmp + 1, filename, buflen - 1) == 0)
                {
                    memcpy(buf, tmp, buflen);
                    flash.readBytes(addr + buflen, buf + buflen, SPIFLASH_PAGE_SIZE - buflen);
                    return pageIdx;
                }
            }
        }
    }

    return 0;
}

uint16_t FS::createMetaPage(const char *filename)
{
    uint16_t page = findFreeMetaPage();

    uint8_t h = fnhash(filename);
    uint16_t buflen = strlen(filename) + 2;

    memset(buf, 0xff, SPIFLASH_PAGE_SIZE);
    buf[0] = 0x01;
    memcpy(buf + 1, filename, buflen - 1);

    flash.writeBytes(pageAddr(page), buf, buflen);
    flash.writeBytes(indexAddr(page), &h, 1);

    return page;
}

File::File(FS &f, uint16_t existing) : fs(f)
{
    metaPage = existing;
    writePage = 0;
    readSize();
    findFirstPage();
    rewind();
}

void File::seek(uint32_t pos)
{
    if (pos == readOffset)
        return;
    if (pos < readOffset)
        rewind();
    read(NULL, pos - readOffset);
}

void File::seekNextPage()
{
    if (readOffset == 0)
    {
        readPage = firstPage;
    }
    else
    {
        uint16_t nextPtr;
        fs.flash.readBytes(fs.nextPtrAddr(readPage), (uint8_t *)&nextPtr, 2);
        if (nextPtr == 0xffff)
            oops();
        readPage = nextPtr;
    }
}

int File::read(void *data, uint32_t len)
{
    if (!metaSize || !len)
        return 0;

    int nread = 0;
    while (len > 0)
    {
        if (readOffset >= metaSize)
            break;
        uint32_t off = readOffset & (SPIFLASH_PAGE_SIZE - 1);
        if (off == 0)
        {
            seekNextPage();
        }
        int n = min(min(len, SPIFLASH_PAGE_SIZE - off), metaSize - readOffset);
        if (data)
        {
            fs.flash.readBytes(fs.pageAddr(readPage) + off, data, n);
            data = (uint8_t *)data + n;
        }
        nread += n;
        len -= n;
        readOffset += n;
    }

    return nread;
}

void File::computeWritePage()
{
    if (isDeleted())
        oops();
    if (writePage)
        return;
    auto prevOff = readOffset;
    auto prevPage = readPage;
    seek(metaSize);
    writePage = readPage;
    readOffset = prevOff;
    readPage = prevPage;
}

void File::appendCore(const void *data, uint32_t len)
{
    if (len == 0)
        return;

    computeWritePage();

    while (len > 0)
    {
        uint32_t off = metaSize & (SPIFLASH_PAGE_SIZE - 1);
        if (off == 0)
            allocatePage();

        LOG("write: left=%d page=0x%x\n", len, writePage);

        int nwrite = min(len, SPIFLASH_PAGE_SIZE - off);
        fs.flash.writeBytes(fs.pageAddr(writePage) + off, data, nwrite);
        len -= nwrite;
        data = (uint8_t *)data + nwrite;
        metaSize += nwrite;
    }
}

void File::append(const void *data, uint32_t len)
{
    appendCore(data, len);
    saveSizeDiff(len);
}

void File::allocatePage()
{
    int start = writePage ? (writePage >> 8) : (metaPage * 13) % fs.numRows;
    uint16_t pageIdx = fs.findFreeDataPage(start);
    if (pageIdx == 0)
        oops(); // out of space
    if (firstPage == 0)
    {
        firstPage = pageIdx;
        fs.flash.readBytes(metaPageAddr() + SNORFS_END_SIZE, fs.buf, SNORFS_REP_START_BLOCK * 2);
        uint16_t *startPtr = (uint16_t *)fs.buf;
        for (int i = 0; i < SNORFS_REP_START_BLOCK; ++i)
        {
            if (startPtr[i] == 0xffff)
            {
                fs.flash.writeBytes(metaPageAddr() + SNORFS_END_SIZE + i * 2, &firstPage, 2);
                startPtr = NULL;
                break;
            }
        }
        if (startPtr)
            oops(); // TODO create new file entry
    }
    else
    {
        if (writePage == 0)
            oops();
        fs.flash.writeBytes(fs.nextPtrAddr(writePage), &pageIdx, 2);
    }
    writePage = pageIdx;
}

void File::truncateCore()
{
    if (firstPage == 0)
        return;

    uint8_t deleted = 0;

    rewind();
    for (uint32_t off = 0; off < metaSize; off += SPIFLASH_PAGE_SIZE)
    {
        seekNextPage();
        fs.flash.writeBytes(fs.indexAddr(readPage), &deleted, 1);
    }

    rewind();
    firstPage = 0;
    writePage = 0;
}

void File::del()
{
    truncateCore();
    uint8_t deleted = 0;
    fs.flash.writeBytes(metaPageAddr(), &deleted, 1);
    fs.flash.writeBytes(fs.indexAddr(metaPage), &deleted, 1);
    metaSize = 0;
    writePage = 0xffff;
}

void File::overwrite(const void *data, uint32_t len)
{
    int32_t sizeDiff = len - metaSize;
    truncateCore();
    appendCore(data, len);
    saveSizeDiff(sizeDiff);
}

#ifdef SNORFS_TEST
void FS::debugDump()
{
    if (numRows == 0)
    {
        LOG("not mounted\n");
        mount();
    }
    LOG("row#: %d; remap: ", numRows);
    for (int i = 0; i < numRows; ++i)
    {
        LOG("%d->%d, ", i, rowRemapCache[i]);
    }
    LOG("\n");
}

void File::debugDump()
{
    LOG("fileID: 0x%x/st:0x%x, rd: 0x%x/%d, wr: 0x%x/%d\n", fileID(), firstPage, readPage, tell(),
        writePage, size());
}
#endif
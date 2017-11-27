#include "SNORFS.h"

/*
TODO:
- keep files in list in FS
- enforce only one handle per file
- nuke caches in files on GC
- implement file data GC
- implement meta-data GC
- try to keep data pages in one block - better for delete?
*/

using namespace codal::snorfs;

#ifdef SNORFS_TEST
#define SNORFS_LEVELING_THRESHOLD 100
#else
#define SNORFS_LEVELING_THRESHOLD 1000
#endif

#define SNORFS_MAGIC 0x3576348e // Random
struct BlockHeader
{
    uint32_t magic;
    uint8_t version;
    uint8_t numMetaRows;
    uint16_t logicalBlockId;
    uint32_t eraseCount;
};

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
    randomSeed = 1;
}

void FS::feedRandom(uint32_t v)
{
    randomSeed ^= v * 0x1000193;
}

// we need a deterministic PRNG - this one has period of 2^32
uint32_t FS::random(uint32_t max)
{
    uint32_t mask = 1;
    while (mask <= max)
        mask = (mask << 1) | 1;
    while (true)
    {
        randomSeed = randomSeed * 1664525 + 1013904223;
        auto v = randomSeed & mask;
        if (v < max)
            return v;
    }
}

int FS::firstFree(uint16_t pageIdx)
{
    flash.readBytes(indexAddr(pageIdx), buf, SPIFLASH_PAGE_SIZE);
    for (int k = 2; k < SPIFLASH_PAGE_SIZE - 2; ++k)
        if (buf[k] == 0xff)
            return pageIdx | k;
    return 0;
}

void FS::progress()
{
    // blink LED or something
}

void FS::format()
{
    uint32_t end = flash.numPages() * SPIFLASH_PAGE_SIZE;
    uint16_t rowIdx = 0;
    BlockHeader hd;
    hd.magic = SNORFS_MAGIC;
    hd.version = 0;
    hd.numMetaRows = 2;
    hd.eraseCount = 0;

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
            hd.logicalBlockId = 0xffff;
        else
            hd.logicalBlockId = rowIdx;
        LOGV("format: %d\n", rowIdx);
        flash.writeBytes(addr, &hd, sizeof(hd));
        rowIdx++;
    }
    progress();
}

void FS::gcCore(bool force, bool isData)
{
    if (!force)
    {
        fullPages = 0;
        deletedPages = 0;
        freePages = 0;
    }

    uint16_t start = 0;
    uint16_t end = numRows;

    // in 'force' mode we're interested in specifically data or meta space
    if (force)
    {
        if (isData)
            start = numMetaRows;
        else
            end = numMetaRows;
    }

    uint32_t maxDelCnt = 0;
    uint32_t maxDelIdx = 0;

    for (unsigned row = start; row < end; ++row)
    {
        uint32_t addr = indexAddr(row << 8);
        flash.readBytes(addr, buf, SPIFLASH_PAGE_SIZE);
        uint16_t numDel = 0;
        for (int i = 2; i < SPIFLASH_PAGE_SIZE - 2; i++)
        {
            if (buf[i] == 0x00)
                numDel++;
            if (!force)
            {
                if (buf[i] == 0x00)
                    deletedPages++;
                else if (buf[i] == 0xff)
                    freePages++;
                else
                    fullPages++;
            }
        }

        LOGV("GC: row=%d del=%d\n", row, numDel);

        if (numDel > maxDelCnt)
        {
            maxDelCnt = numDel;
            maxDelIdx = row;
        }
    }

    if (force && !maxDelCnt)
    {
        if (isData)
            LOG("out of data space\n");
        else
            LOG("out of meta space\n");
        oops(); // really out of space!
    }

    LOG("GC: ");

    // we do a GC when either one is true:
    //   * force is true (we desperately need space)
    //   * there's a row that's more than 50% deleted
    //   * clearing a row will increase free space by more than 20%
    if (force || maxDelCnt > SPIFLASH_PAGE_SIZE / 2 || (maxDelCnt * 5 > freePages))
    {
        swapRow(rowRemapCache[maxDelIdx]);
        if (!readHeaders()) // this will trigger levelling on the new free block
            oops();         // but it should never fail
    }

    dump();
}

void FS::swapRow(int row)
{
    LOG("[swap row: %d] ", row);
    if (freeRow == row || row > numRows)
        oops();
    uint32_t trg = freeRow * SPIFLASH_BIG_ROW_SIZE;
    uint32_t src = row * SPIFLASH_BIG_ROW_SIZE;

    uint32_t skipmask[SPIFLASH_PAGE_SIZE / 32];
    memset(skipmask, 0, sizeof(skipmask));
    flash.readBytes(src + SPIFLASH_PAGE_SIZE, buf, SPIFLASH_PAGE_SIZE);
    for (int i = 2; i < SPIFLASH_PAGE_SIZE - 2; i++)
    {
        if (buf[i] == 0x00)
        {
            skipmask[i / 32] |= 1U << (i % 32);
            buf[i] = 0xff;
            deletedPages--;
            freePages++;
        }
    }
    flash.writeBytes(trg + SPIFLASH_PAGE_SIZE, buf, SPIFLASH_PAGE_SIZE);
    for (int i = 2; i < SPIFLASH_PAGE_SIZE - 2; ++i)
    {
        if (skipmask[i / 32] & (1U << (i % 32)))
            continue;

        flash.readBytes(src + SPIFLASH_PAGE_SIZE * i, buf, SPIFLASH_PAGE_SIZE);
        flash.writeBytes(trg + SPIFLASH_PAGE_SIZE * i, buf, SPIFLASH_PAGE_SIZE);
    }

    for (int i = 0; i < 2; ++i)
    {
        uint32_t off = SPIFLASH_BIG_ROW_SIZE - SPIFLASH_PAGE_SIZE * (2 - i);
        flash.readBytes(src + off, buf, SPIFLASH_PAGE_SIZE);
        auto ptr = (uint16_t *)(void *)buf;
        int off2 = i * (SPIFLASH_PAGE_SIZE / 2);
        for (int j = 0; j < SPIFLASH_PAGE_SIZE / 2; ++j)
        {
            int jj = j + off2;
            if (skipmask[jj / 32] & (1U << (jj % 32)))
                ptr[j] = 0xffff;
        }
        flash.writeBytes(trg + off, buf, SPIFLASH_PAGE_SIZE);
    }

    flash.readBytes(src, buf, SPIFLASH_PAGE_SIZE);
    auto hd = (BlockHeader *)(void *)buf;
    flash.writeBytes(trg + 6, &hd->logicalBlockId, 2);
    hd->logicalBlockId = 0xffff;
    hd->eraseCount++;
    flash.eraseBigRow(src);
    int last = 0;
    for (int i = 0; i < SPIFLASH_PAGE_SIZE; ++i)
        if (buf[i] != 0xff)
            last = i;
    flash.writeBytes(src, buf, last + 1);

    for (int i = 0; i < numRows; ++i)
    {
        if (rowRemapCache[i] == row)
            rowRemapCache[i] = freeRow;
    }
    freeRow = row; // new free row
}

bool FS::readHeaders()
{
    memset(rowRemapCache, 0xff, numRows);

    BlockHeader hd;
    int freeRow = -1;

    int minEraseIdx = -1;
    uint32_t minEraseCnt = 0;
    uint32_t freeEraseCnt = 0;

    for (unsigned i = 0; i < numRows + 1; ++i)
    {
        auto addr = i * SPIFLASH_BIG_ROW_SIZE;
        flash.readBytes(addr, &hd, sizeof(hd));
        if (hd.magic != SNORFS_MAGIC || hd.version != 0)
            return false;

        numMetaRows = hd.numMetaRows;

        if (hd.logicalBlockId == 0xffff)
        {
            if (freeRow == -1)
                freeRow = i;
            else
                return false;
            freeEraseCnt = hd.eraseCount;
        }
        else
        {
            if (hd.logicalBlockId >= numRows || rowRemapCache[hd.logicalBlockId] != 0xff)
                return false;
            rowRemapCache[hd.logicalBlockId] = i;
            if (minEraseIdx < 0 || hd.eraseCount < minEraseCnt)
            {
                minEraseCnt = hd.eraseCount;
                minEraseIdx = i;
            }
        }
    }

    this->freeRow = freeRow;

    if (freeRow == -1 || !numMetaRows || numMetaRows > numRows / 2)
        return false;

    if (minEraseCnt + SNORFS_LEVELING_THRESHOLD < freeEraseCnt)
    {
        swapRow(minEraseIdx);
        LOG(" for level\n");
    }
    else
    {
        LOGV("[no level swap: free %d, min %d]", freeEraseCnt, minEraseCnt);
    }

    return true;
}

void FS::mount()
{
    if (numRows > 0)
        return;

    numRows = (flash.numPages() / SPIFLASH_BIG_ROW_PAGES) - 1;
    rowRemapCache = new uint8_t[numRows];

    if (!readHeaders())
    {
        format();
        if (!readHeaders())
            oops();
    }
    maybeGC();
}

uint16_t FS::findFreePage(bool isData, uint16_t hint)
{
    bool wrapped = false;
    bool gc = false;
    uint16_t start = isData ? numMetaRows : 0;
    uint16_t end = isData ? numRows : numMetaRows;
    uint16_t ptr = random(end - start) + start;

    uint16_t fr;

    if (hint != 0)
    {
        fr = firstFree(hint & 0xff00);
        if (fr)
            return fr;
    }

    for (;;)
    {
        fr = firstFree(ptr << 8);
        if (fr)
            return fr;
        if (++ptr == end)
        {
            if (wrapped)
            {
                if (gc)
                    oops();
                gcCore(true, isData);
                gc = true;
            }
            ptr = start;
            wrapped = true;
        }
    }
}

FS::~FS()
{
    delete rowRemapCache;
}

void FS::markPage(uint16_t page, uint8_t flag)
{
    if (flag == 0xff)
        oops();
    if (flag == 0)
    {
        deletedPages++;
        fullPages--;
    }
    else
    {
        fullPages++;
        freePages--;
    }
    flash.writeBytes(indexAddr(page), &flag, 1);
}

void File::newMetaPage()
{
    uint32_t prevPage = metaPage;
    uint32_t prevSize = metaSize;

    fs.flash.readBytes(fs.pageAddr(prevPage), fs.buf, 64);
    fs.buf[64] = 0;
    const char *fn = (const char *)fs.buf + 1;
    int fnlen = strlen(fn);
    memset(fs.buf + fnlen + 2, 0xff, SPIFLASH_PAGE_SIZE - (fnlen + 2));
    readSize();
    if (metaSize != 0)
        oops();

    metaPage = fs.findFreePage(false);

    fs.flash.readBytes(fs.pageAddr(prevPage), fs.buf, fnlen + 2);
    fs.flash.writeBytes(metaPageAddr(), fs.buf, fnlen + 2);
    metaSize = prevSize;
    saveSizeDiff(metaSize);
    fs.flash.writeBytes(metaPageAddr() + SNORFS_END_SIZE, &firstPage, 2);

    uint8_t zero = 0;
    fs.flash.writeBytes(fs.pageAddr(prevPage), &zero, 1);
    fs.markPage(prevPage, 0);
    fs.markPage(metaPage, fnhash(fn));
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
    LOGV("WRSZ: num=%d at %d: 0x%x 0x%x, meta=%x\n", num, metaSizeOff, buf[0] ^ 0xff, buf[1] ^ 0xff,
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
    uint8_t h = fnhash(filename);
    feedRandom(h);
    uint16_t page = findFreePage(false);
    uint16_t buflen = strlen(filename) + 2;

    memset(buf, 0xff, SPIFLASH_PAGE_SIZE);
    buf[0] = 0x01;
    memcpy(buf + 1, filename, buflen - 1);

    flash.writeBytes(pageAddr(page), buf, buflen);
    markPage(page, h);

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

        LOGV("write: left=%d page=0x%x\n", len, writePage);

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
    fs.feedRandom(fileID());
    // if writePage is set, try to keep the new page on the same row - this helps with delete locality
    uint16_t pageIdx = fs.findFreePage(true, writePage);
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
        {
            newMetaPage();
        }
    }
    else
    {
        if (writePage == 0)
            oops();
        fs.flash.writeBytes(fs.nextPtrAddr(writePage), &pageIdx, 2);
    }

    fs.markPage(pageIdx, 1);
    writePage = pageIdx;
}

void File::truncateCore()
{
    if (firstPage == 0)
        return;

    rewind();
    for (readOffset = 0; readOffset < metaSize; readOffset += SPIFLASH_PAGE_SIZE)
    {
        seekNextPage();
        fs.markPage(readPage, 0);
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
    fs.markPage(metaPage, 0);
    metaSize = 0;
    writePage = 0xffff;
}

void File::overwrite(const void *data, uint32_t len)
{
    int32_t sizeDiff = len - metaSize;
    auto prevID = fileID();
    truncateCore();
    metaSize = 0;
    appendCore(data, len);
    // if the meta-page was reallocated, the size listed there is now zero
    if (prevID != fileID())
        sizeDiff = len;
    saveSizeDiff(sizeDiff);
}

#ifdef SNORFS_TEST
void FS::dump()
{
    if (numRows == 0)
    {
        LOG("not mounted\n");
        mount();
    }
    LOG("row#: %d; remap: ", numRows);

    for (unsigned i = 0; i < numRows + 1; ++i)
    {
        BlockHeader hd;
        auto addr = i * SPIFLASH_BIG_ROW_SIZE;
        flash.readBytes(addr, &hd, sizeof(hd));
        LOG("[%d: %d] ", (int16_t)hd.logicalBlockId, hd.eraseCount);
    }

    LOG("free: %d/%d, (junk: %d)", freePages + deletedPages, fullPages + freePages + deletedPages,
        deletedPages);
    LOG("\n");
}

void FS::debugDump()
{
    // dump();
}

void File::debugDump()
{
    LOGV("fileID: 0x%x/st:0x%x, rd: 0x%x/%d, wr: 0x%x/%d\n", fileID(), firstPage, readPage, tell(),
         writePage, size());
}
#endif
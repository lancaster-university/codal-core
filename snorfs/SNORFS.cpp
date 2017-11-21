#include "SNORFS.h"

/*
TODO:
- seperate pointer for append
- keep files in list in FS
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
    numDataRows = 0;
}

int FS::firstFree(uint32_t addr, int startOff)
{
    flash.readBytes(addr, buf, SPIFLASH_PAGE_SIZE);
    uint8_t firstFree = 0;
    for (int k = startOff; k < SPIFLASH_PAGE_SIZE; ++k)
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

void FS::mount()
{
    if (numDataRows > 0)
        return;

    numDataRows = (flash.numPages() / SPIFLASH_BIG_ROW_PAGES) - SNORFS_META_ROWS - 1;
    rowRemapCache = new uint8_t[numDataRows];

    for (int i = 0; i < SNORFS_META_ROWS; ++i)
    {
        // last page in meta row is index
        int startPos = i == 0 ? SNORFS_RESERVED_META_PAGES : 0;
        metaFree[i] = firstFree(metaIdxAddr(i), startPos);
    }

    uint32_t remapSize = getRemapSize();
    uint32_t addr = 0;
    for (int i = 0; i < numDataRows; ++i)
    {
        rowRemapCache[i] = SNORFS_META_ROWS + i; // default
        flash.readBytes(addr, buf, remapSize);
        for (unsigned k = 0; k < remapSize; ++k)
        {
            if (buf[k] == 0xff)
                break;
            rowRemapCache[i] = buf[k] + 1;
        }
        addr += remapSize;
    }
}

uint16_t FS::findFreeDataPage(int startRow)
{
    bool wrapped = false;
    int ptr = startRow;
    for (;;)
    {
        int fr = firstFree(dataIndexAddr(ptr << 8), 1);
        if (fr < SPIFLASH_BIG_ROW_PAGES - 2)
        {
            return (ptr << 8) | fr;
        }
        ptr++;
        if (ptr == numDataRows)
        {
            if (wrapped)
                return 0;
            ptr = 0;
            wrapped = true;
        }
    }
}

FS::~FS()
{
    delete rowRemapCache;
}

void File::findFreeMetaPage()
{
    for (metaRow = 0; metaRow < SNORFS_META_ROWS; ++metaRow)
    {
        // last page is index
        if (fs.metaFree[metaRow] < SPIFLASH_BIG_ROW_PAGES - 1)
        {
            metaPage = fs.metaFree[metaRow]++;
            break;
        }
    }
    if (metaRow >= SNORFS_META_ROWS)
        oops(); // out of meta space
}

void File::newMetaPage()
{
    uint32_t prevAddr = metaPageAddr();
    uint32_t prevIdx = fs.metaIdxAddr(metaRow) + metaPage;
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

    findFreeMetaPage();
    fs.flash.writeBytes(metaPageAddr(), fs.buf, fnlen + 2);
    metaSize = prevSize;
    saveSizeDiff(metaSize);
    fs.flash.writeBytes(metaPageAddr() + SNORFS_END_SIZE, &firstPage, 2);

    uint8_t zero = 0;
    fs.flash.writeBytes(prevAddr, &zero, 1);
    fs.flash.writeBytes(prevIdx, &zero, 1);
    fs.flash.writeBytes(fs.metaIdxAddr(metaRow) + metaPage, &h, 1);
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

File::File(FS &f, const char *filename) : fs(f)
{
    bool found = false;

    fs.mount();

    uint8_t h = fnhash(filename);
    uint16_t buflen = strlen(filename) + 2;

    if (buflen > 64)
        oops();

    for (int i = 0; i < SNORFS_META_ROWS; ++i)
    {
        if (!fs.metaFree[i])
            continue;
        fs.flash.readBytes(fs.metaIdxAddr(i), fs.buf, fs.metaFree[i]);
        for (int j = i == 0 ? SNORFS_RESERVED_META_PAGES : 0; j < fs.metaFree[i]; ++j)
        {
            if (fs.buf[j] == h)
            {
                uint8_t tmp[buflen];
                fs.flash.readBytes(fs.metaPageAddr(i, j), tmp, buflen);
                if (tmp[0] != 0 && memcmp(tmp + 1, filename, buflen - 1) == 0)
                {
                    memcpy(fs.buf, tmp, buflen);
                    fs.flash.readBytes(fs.metaPageAddr(i, j) + buflen, fs.buf + buflen,
                                       SPIFLASH_PAGE_SIZE - buflen);
                    found = true;
                    metaRow = i;
                    metaPage = j;
                    break;
                }
            }
        }
    }

    if (!found)
    {
        findFreeMetaPage();

        memset(fs.buf, 0xff, SPIFLASH_PAGE_SIZE);
        fs.buf[0] = 0x01;
        memcpy(fs.buf + 1, filename, buflen - 1);

        fs.flash.writeBytes(metaPageAddr(), fs.buf, buflen);
        fs.flash.writeBytes(fs.metaIdxAddr(metaRow) + metaPage, &h, 1);
    }

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

void File::seekToStableAddr(uint16_t nextPtr)
{
    fs.flash.readBytes(fs.dataIndexAddr(nextPtr & 0xff00), fs.buf, SPIFLASH_PAGE_SIZE);
    for (int i = 0; i < SPIFLASH_PAGE_SIZE; ++i)
    {
        if (fs.buf[i] == (nextPtr & 0xff))
        {
            readPage = (nextPtr & 0xff00) | i;
            return;
        }
    }
    oops();
}

void File::seekNextPage()
{
    if (readOffset == 0)
    {
        seekToStableAddr(firstPage);
    }
    else
    {
        uint16_t nextPtr;
        fs.flash.readBytes(fs.dataNextPtrAddr(readPage), (uint8_t *)&nextPtr, 2);
        if ((nextPtr & 0xff00) == 0xff00)
        {
            readPage += nextPtr & 0xff;
        }
        else
        {
            seekToStableAddr(nextPtr);
        }
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
            fs.flash.readBytes(fs.dataDataAddr(readPage) + off, data, n);
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
    if (writePage)
        return;
    auto prevOff = readOffset;
    auto prevPage = readPage;
    seek(metaSize);
    writePage = readPage;
    readOffset = prevOff;
    readPage = prevPage;
}

void File::append(const void *data, uint32_t len)
{
    if (len == 0)
        return;

    computeWritePage();

    auto len0 = len;

    while (len > 0)
    {
        uint32_t off = metaSize & (SPIFLASH_PAGE_SIZE - 1);
        if (off == 0)
            allocatePage();

        LOG("write: left=%d page=0x%x\n", len, writePage);

        int nwrite = min(len, SPIFLASH_PAGE_SIZE - off);
        fs.flash.writeBytes(fs.dataDataAddr(writePage) + off, data, nwrite);
        len -= nwrite;
        data = (uint8_t *)data + nwrite;
        metaSize += nwrite;
    }

    saveSizeDiff(len0);
}

uint16_t File::stablePageAddr(uint16_t pageIdx)
{
    fs.flash.readBytes(fs.dataIndexAddr(pageIdx & 0xff00), fs.buf, SPIFLASH_PAGE_SIZE);
    uint8_t blockID = metaPage * 13 + pageIdx * 17;
    if (blockID < 2 || blockID == 0xff)
        blockID = 2;
    for (;;)
    {
        for (int i = 0; i < SPIFLASH_PAGE_SIZE; ++i)
        {
            if (fs.buf[i] == blockID)
                goto nextOne;
        }
        break;
    nextOne:
        blockID++;
        if (blockID == 0xff)
            blockID = 2;
    }

    fs.flash.writeBytes(fs.dataIndexAddr(pageIdx), &blockID, 1);
    return (pageIdx & 0xff00) | blockID;
}

void File::allocatePage()
{
    int start = writePage ? (writePage >> 8) : (metaPage * 13 + metaRow) % fs.numDataRows;
    int pageIdx = fs.findFreeDataPage(start);
    if (pageIdx == 0)
        oops(); // out of space
    if (firstPage == 0)
    {
        firstPage = stablePageAddr(pageIdx);
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
        uint16_t off;
        if ((writePage >> 8) == (pageIdx >> 8))
        {
            off = pageIdx - writePage;
            if (off & 0xff00)
                oops();
            off |= 0xff00;
            uint8_t noId = 1;
            fs.flash.writeBytes(fs.dataIndexAddr(pageIdx), &noId, 1);
        }
        else
        {
            off = stablePageAddr(pageIdx);
        }
        fs.flash.writeBytes(fs.dataNextPtrAddr(writePage), &off, 2);
    }
    writePage = pageIdx;
}

    /*
    void File::truncate()
    {
        rewind();

    }

    void File::remove()
    {
        truncate();
    }
    */

#ifdef SNORFS_TEST
void FS::debugDump()
{
    if (numDataRows == 0)
    {
        LOG("not mounted\n");
        mount();
    }
    LOG("row#: %d; remap: ", numDataRows);
    for (int i = 0; i < numDataRows; ++i)
    {
        LOG("%d->%d, ", i, rowRemapCache[i]);
    }
    LOG("META PTRS:");
    for (int i = 0; i < SNORFS_META_ROWS; ++i)
        LOG(" %d", metaFree[i]);
    LOG("\n");
}

void File::debugDump()
{
    LOG("fileID: 0x%x/st:0x%x, rd: 0x%x/%d, wr: 0x%x/%d\n", fileID(), firstPage, readPage,
        tell(), writePage, size());
}
#endif
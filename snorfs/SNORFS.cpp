#include "SNORFS.h"

using namespace codal::snorfs;

#define SNORFS_RESERVED_META_PAGES (SPIFLASH_SMALL_ROW_PAGES)
#define SNORFS_REP_START_BLOCK 32
#define SNORFS_END_SIZE (SPIFLASH_PAGE_SIZE - SNORFS_REP_START_BLOCK * 2)

static uint8_t fnhash(const char *fn)
{
    uint32_t h = 2166136261;
    while (*fn)
        h = (h * 16777619) ^ (uint8_t)*fn++;
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
    rowRemapCache = new uint8_t[3 * numDataRows];

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

void File::updateSize(uint32_t newSize)
{
    int sizeDiff = newSize - metaSize;
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
        int b0 = sizeDiff >> 14;
        if (b0 != -1 && b0 != 0)
            buf[num++] = b0;
        buf[num++] = 0x80 | (sizeDiff >> 7);
        buf[num++] = sizeDiff & 0x7f;
    }
    for (int i = 0; i < num; ++i)
        buf[i] ^= 0xff;
    if (metaSizeOff + num >= SNORFS_END_SIZE)
        oops(); // TODO re-allocate a new meta page
    fs.flash.writeBytes(metaPageAddr() + metaSizeOff, buf, num);
    metaSizeOff += num;
    metaSize = newSize;
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
        if (!c)
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

    metaSizeOff = fs.buf - buf;
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
    currSeekPage = firstPage;
    currSeekOffset = 0;
}

File::File(FS &f, const char *filename) : fs(f)
{
    bool found = false;

    fs.mount();

    uint8_t h = fnhash(filename);
    uint16_t buflen = strlen(filename) + 2;

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
                    fs.flash.readBytes(fs.metaPageAddr(i, j) + buflen, fs.buf,
                                       SPIFLASH_PAGE_SIZE - buflen);
                    found = true;
                    metaRow = i;
                    metaPage = j;
                }
            }
        }
    }

    if (!found)
    {
        memset(fs.buf, 0xff, SPIFLASH_PAGE_SIZE);
        fs.buf[0] = 0x01;
        memcpy(fs.buf + 1, filename, buflen - 1);
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

        fs.flash.writeBytes(metaPageAddr(), fs.buf, buflen);
        fs.flash.writeBytes(fs.metaIdxAddr(metaRow) + metaRow, &h, 1);
    }

    readSize();
    findFirstPage();
    rewind();
}

void File::seek(uint32_t pos)
{
    if (pos == currSeekOffset)
        return;
    if (pos < currSeekOffset)
        rewind();
    read(NULL, pos - currSeekOffset);
}

void File::seekNextPage()
{
    if (currSeekOffset == 0)
        currSeekPage = firstPage;
    else
    {
        uint16_t nextPtr;
        fs.flash.readBytes(fs.dataNextPtrAddr(currSeekPage), (uint8_t *)&nextPtr, 2);
        if ((nextPtr & 0xff00) == 0xff00)
        {
            currSeekPage += nextPtr & 0xff;
        }
        else
        {
            fs.flash.readBytes(fs.dataIndexAddr(nextPtr & 0xff00), fs.buf, SPIFLASH_PAGE_SIZE);
            for (int i = 0; i < SPIFLASH_PAGE_SIZE; ++i)
            {
                if (fs.buf[i] == (nextPtr & 0xff))
                {
                    currSeekPage = (nextPtr & 0xff00) | i;
                    return;
                }
            }
            oops();
        }
    }
}

int File::read(uint8_t *data, uint32_t len)
{
    if (!metaSize || !len)
        return 0;

    int nread = 0;
    while (len > 0)
    {
        if (currSeekOffset >= metaSize)
            break;
        uint32_t off = currSeekOffset & (SPIFLASH_PAGE_SIZE - 1);
        if (off == 0)
        {
            seekNextPage();
        }
        int n = min(min(len, SPIFLASH_PAGE_SIZE - off), metaSize - currSeekOffset);
        if (data)
        {
            fs.flash.readBytes(fs.dataDataAddr(currSeekPage) + off, data, n);
            data += n;
        }
        nread += n;
        len -= n;
        currSeekOffset += n;
    }

    return nread;
}

void File::append(const uint8_t *data, uint32_t len)
{
    if (currSeekOffset != metaSize)
    {
        seek(metaSize);
    }

    if (len == 0)
        return;

    uint32_t prevSize = metaSize;

    while (len > 0)
    {
        uint32_t off = metaSize & (SPIFLASH_PAGE_SIZE - 1);
        if (off == 0)
            allocatePage();

        int nwrite = min(len, SPIFLASH_PAGE_SIZE - off);
        fs.flash.writeBytes(fs.dataDataAddr(currSeekPage), data, nwrite);
        len -= nwrite;
        data += nwrite;
        metaSize += nwrite;
        currSeekOffset += nwrite;
    }

    metaSize = prevSize;
    updateSize(currSeekOffset);
    if (currSeekOffset != metaSize)
        oops();
}

void File::allocatePage()
{
    if (currSeekOffset != metaSize)
        oops();
    int start = currSeekPage ? (currSeekPage >> 8) : (metaPage * 13 + metaRow) % fs.numDataRows;
    int pageIdx = fs.findFreeDataPage(start);
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
                startPtr[i] = pageIdx;
                fs.flash.writeBytes(metaPageAddr() + SNORFS_END_SIZE + i * 2, &startPtr[i], 2);
                startPtr = NULL;
                break;
            }
        }
        if (startPtr)
            oops(); // TODO create new file entry
    }
    else
    {
        if (currSeekPage == 0)
            oops();
        uint16_t off;
        uint8_t blockID = 1;
        if ((currSeekPage >> 8) == (pageIdx >> 8))
        {
            off = pageIdx - currSeekPage;
            if (off & 0xff00)
                oops();
            off |= 0xff00;
        }
        else
        {
            fs.flash.readBytes(fs.dataIndexAddr(pageIdx & 0xff00), fs.buf, SPIFLASH_PAGE_SIZE);
            blockID = metaPage * 13 + pageIdx * 17;
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
            off = (pageIdx & 0xff00) | blockID;
        }
        fs.flash.writeBytes(fs.dataIndexAddr(pageIdx), &blockID, 1);
        fs.flash.writeBytes(fs.dataNextPtrAddr(currSeekPage), &off, 2);
    }
    currSeekPage = pageIdx;
}

#ifdef SNORFS_TEST
void FS::debugDump()
{
    if (numDataRows == 0)
    {
        printf("not mounted\n");
        mount();
    }
    printf("row#: %d; remap: ", numDataRows);
    for (int i = 0; i < numDataRows; ++i)
    {
        printf("%d->%d, ", i, rowRemapCache[i]);
    }
    printf("META PTRS:");
    for (int i = 0; i < SNORFS_META_ROWS; ++i)
        printf(" %d", metaFree[i]);
    printf("\n");
}
#endif
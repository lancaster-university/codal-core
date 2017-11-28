#include "SNORFS-test.h"
#include "SNORFS.h"

#include <string.h>
#include <stdio.h>

#include <vector>

#define oops() assert(false)

#define MAX_WR 2000000

typedef codal::snorfs::File File;
codal::snorfs::FS *fs;

class FileCache
{
public:
    char name[64];
    bool del;
    vector<uint8_t> data;

    void append(const void *buf, uint32_t len)
    {
        assert(len < MAX_WR);
        uint8_t *ptr = (uint8_t *)buf;
        while (len--)
            data.push_back(*ptr++);
    }

    void validate(File *f)
    {
        uint8_t tmp[512];
        bool align = false;
        f->seek(0);
        uint32_t ptr = 0;

        assert(!del);

        while (ptr < data.size())
        {
            assert(ptr == f->tell());

            size_t len;

            if (align)
            {
                len = 256;
                align = false;
            }
            else if (rand() % 3 == 0)
            {
                len = 256 - (ptr & 0xff);
                if (len == 0)
                    len = 256;
                align = true;
            }
            else
            {
                len = rand() % 500 + 1;
            }
            len = min(data.size() - ptr, len);

            memset(tmp, 0, sizeof(tmp));

            int l = f->read(tmp, len);
            LOGV("read len=%d l=%d at %d / %x %x %x %x\n", (int)len, l, ptr, tmp[0], tmp[1], tmp[2],
                 tmp[3]);
            f->debugDump();
            assert(l == (int)len);
            for (unsigned i = 0; i < len; ++i)
            {
                if (tmp[i] != data[ptr + i])
                {
                    LOG("failure: %d != %d at %d (i=%d)\n", tmp[i], data[ptr + i], ptr + i, i);
                    assert(false);
                }
            }
            ptr += len;
        }
        int l = f->read(tmp, 1);
        assert(l == 0);

        // test reading random ranges
        int ranges = 5;
        while (ranges--)
        {
            int start = rand() % data.size();
            size_t len = rand() % 512;
            len = min(len, data.size() - start);
            f->seek(start);
            f->read(tmp, len);
            for (unsigned i = 0; i < len; ++i)
            {
                if (tmp[i] != data[start + i])
                {
                    assert(false);
                }
            }
        }
    }
};

vector<FileCache *> files;

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
            assert(data[addr + i] == 0xff || (data[addr + i] && *ptr == 0x00));
            data[addr + i] = *ptr++;
        }
        return 0;
    }
    int eraseSmallRow(uint32_t addr) { return erase(addr, SPIFLASH_SMALL_ROW_SIZE); }
    int eraseBigRow(uint32_t addr) { return erase(addr, SPIFLASH_BIG_ROW_SIZE); }
    int eraseChip() { return erase(0, chipSize()); }
};

File *mk(const char *fn)
{
    auto r = fs->open(fn);
    assert(r != NULL);
    return r;
}

uint8_t randomData[1024 * 1024 * 16];
uint32_t fileSeqNo;

const char *getFileName(uint32_t id)
{
    static char namebuf[40];
    id *= 0x811c9dc5;
    snprintf(namebuf, sizeof(namebuf), "%x.dat", id);
    return namebuf;
}

FileCache *lookupFile(const char *fn)
{
    for (auto f : files)
    {
        if (strcmp(f->name, fn) == 0)
            return f;
    }
    auto r = new FileCache();
    files.push_back(r);
    strcpy(r->name, fn);
    return r;
}

uint8_t *getRandomData()
{
    return randomData + rand() % (sizeof(randomData) - MAX_WR);
}

void simpleTest(const char *fn, int len, int rep = 1)
{
    if (fn == NULL)
        fn = getFileName(++fileSeqNo);

    LOGV("\n\n* %s\n", fn);

    auto fc = lookupFile(fn);

    auto f = mk(fn);
    f->debugDump();
    while (rep--)
    {
        auto data = getRandomData();
        f->append(data, len);
        f->debugDump();
        if (rep % 32 == 7)
        {
            delete f;
            f = mk(fn);
        }
        fc->append(data, len);
        f->debugDump();
    }

    fc->validate(f);
    delete f;

    LOGV("\nAgain.\n");

    f = mk(fn);
    fc->validate(f);
    delete f;
}

void multiTest(int nfiles, int blockSize, int reps, bool over = false)
{
    auto fs = new File *[nfiles];
    auto fcs = new FileCache *[nfiles];
    for (int i = 0; i < nfiles; ++i)
    {
        fcs[i] = lookupFile(getFileName(++fileSeqNo));
        fs[i] = mk(fcs[i]->name);
    }
    reps *= nfiles;
    while (reps--)
    {
        int i = rand() % nfiles;
        auto d = getRandomData();
        auto len = (rand() % blockSize) + 1;
        if (over)
        {
            fs[i]->overwrite(d, len);
            fcs[i]->data.clear();
        }
        else
        {
            fs[i]->append(d, len);
        }
        fcs[i]->append(d, len);
    }
    for (int i = 0; i < nfiles; ++i)
    {
        fcs[i]->validate(fs[i]);
    }
    for (int i = 0; i < nfiles; ++i)
    {
        if (rand() % 10 != 0)
        {
            fs[i]->del();
            fcs[i]->del = true;
        }
        delete fs[i];
    }
}

void testAll()
{
    for (auto fc : files)
    {
        if (fc->del)
        {
            assert(!fs->exists(fc->name));
        }
        else
        {
            auto f = fs->open(fc->name, false);
            assert(!!f);
            fc->validate(f);
            delete f;
        }
    }
}

int main()
{
    for (uint32_t i = 0; i < sizeof(randomData); ++i)
        randomData[i] = rand();
    MemFlash flash(2 * 1024 * 1024 / SPIFLASH_PAGE_SIZE);
    fs = new codal::snorfs::FS(flash);
    fs->dump();
    simpleTest("hello.txt", 100);
    fs->debugDump();
    simpleTest(NULL, 1000);
    simpleTest(NULL, 256);
    simpleTest(NULL, 10000);
    simpleTest(NULL, 400000);
    simpleTest(NULL, 100, 20);
    simpleTest(NULL, 128, 20);
    simpleTest(NULL, 128, 2000);
    simpleTest(NULL, 13, 2000);
    simpleTest(NULL, 1003, 20);
    testAll();

    multiTest(2, 1000, 100);
    multiTest(10, 1000, 100);
    for (int i = 0; i < 20; ++i)
        multiTest(10, 300, 200);
    simpleTest(NULL, 1003, 300);
    testAll();

    multiTest(10, 1000, 10000, true);
    testAll();

    fs->dump();

    // re-mount
    fs = new codal::snorfs::FS(flash);
    fs->dump();
    testAll();

    printf("OK\n");

    return 0;
}
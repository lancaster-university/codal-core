#include "SNORFS.h"

#include <string.h>
#include <stdio.h>

#include <vector>

#define oops() assert(false)

typedef codal::snorfs::File File;
codal::snorfs::FS *fs;

class FileCache
{
public:
    char name[64];
    vector<uint8_t> data;

    void append(const void *buf, uint32_t len)
    {
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
            LOG("read len=%d at %d / %x %x %x %x\n", (int)len, ptr, tmp[0], tmp[1], tmp[2], tmp[3]);
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
    }
};

vector<FileCache> files;

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

File *mk(const char *fn)
{
    return new File(*fs, fn);
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
    for (auto &f : files)
    {
        if (strcmp(f.name, fn) == 0)
            return &f;
    }
    files.push_back(FileCache());
    auto r = &files.back();
    strcpy(r->name, fn);
    return r;
}

void simpleTest(const char *fn, const void *data, int len, int rep = 1)
{
    if (fn == NULL)
        fn = getFileName(++fileSeqNo);

    LOG("\n\n* %s\n", fn);

    if (data == NULL)
    {
        data = randomData + rand() % (sizeof(randomData) - len - 1);
    }

    auto fc = lookupFile(fn);

    auto f = mk(fn);
    f->debugDump();
    while (rep--)
    {
        f->append(data, len);
        fc->append(data, len);
    }
    f->debugDump();

    fc->validate(f);
    delete f;

    LOG("\nAgain.\n");

    f = mk(fn);
    fc->validate(f);
    delete f;
}

void testAll() {
    for (auto &fc : files)
    {
        auto f = mk(fc.name);
        fc.validate(f);
        delete f;
    }
}

int main()
{
    for (uint32_t i = 0; i < sizeof(randomData); ++i)
        randomData[i] = rand();
    MemFlash flash(1024 * 1024 / SPIFLASH_PAGE_SIZE);
    flash.eraseChip();
    fs = new codal::snorfs::FS(flash);
    fs->debugDump();
    simpleTest("hello.txt", NULL, 100);
    fs->debugDump();
    simpleTest(NULL, NULL, 1000);
    simpleTest(NULL, NULL, 256);
    simpleTest(NULL, NULL, 10000);
    simpleTest(NULL, NULL, 400000);
    simpleTest(NULL, NULL, 100, 20);
    simpleTest(NULL, NULL, 128, 20);
    testAll();
    printf("OK\n");

    return 0;
}
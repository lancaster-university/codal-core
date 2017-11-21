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

            int l = f->read(tmp, len);
            assert(l == (int)len);
            for (unsigned i = 0; i < len; ++i)
            {
                assert(tmp[i] == data[ptr + i]);
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

uint8_t randomData[256 * 256 * 4];
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

void simpleTest(const char *fn, const void *data, int len)
{
    if (fn == NULL)
        fn = getFileName(++fileSeqNo);

    if (data == NULL)
    {
        data = randomData + rand() % (sizeof(randomData) - len - 1);
    }

    auto fc = lookupFile(fn);

    auto f = mk(fn);
    f->debugDump();
    f->append(data, len);
    fc->append(data, len);
    f->debugDump();

    fc->validate(f);
    delete f;

    f = mk(fn);
    fc->validate(f);
    delete f;
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
    printf("OK\n");

    return 0;
}
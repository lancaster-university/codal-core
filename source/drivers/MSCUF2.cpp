#include "MSCUF2.h"
#include "FAT.h"

#if CONFIG_ENABLED(DEVICE_USB)

#define UF2_DEFINE_HANDOVER 1
#include "uf2format.h"

#include "CodalCompat.h"
#include "CodalDmesg.h"

#define NUM_FAT_BLOCKS 65500

#define UF2_SIZE (flashSize() * 2)
#define UF2_SECTORS (UF2_SIZE / 512)
#define UF2_FIRST_SECTOR (numTextFiles() + 2)
#define UF2_LAST_SECTOR (UF2_FIRST_SECTOR + UF2_SECTORS - 1)

#define SECTORS_PER_FAT FAT_SECTORS_PER_FAT(NUM_FAT_BLOCKS)
#define START_FAT0 FAT_START_FAT0(NUM_FAT_BLOCKS)
#define START_FAT1 FAT_START_FAT1(NUM_FAT_BLOCKS)
#define START_ROOTDIR FAT_START_ROOTDIR(NUM_FAT_BLOCKS)
#define START_CLUSTERS FAT_START_CLUSTERS(NUM_FAT_BLOCKS)

#define LOG DMESG

namespace codal
{

uint32_t MSCUF2::getCapacity()
{
    return NUM_FAT_BLOCKS;
}

static int numClusters(UF2FileEntry *p)
{
    return (int)(p->size + 511) / 512;
}

static int numDirEntries(UF2FileEntry *p)
{
    return 1 + (strlen(p->filename) + 1 + 12) / 13;
}

static uint8_t fatChecksum(const char *name)
{
    uint8_t sum = 0;
    for (int i = 0; i < 11; ++i)
        sum = ((sum & 1) << 7) + (sum >> 1) + *name++;
    return sum;
}

// note that ptr might be unaligned
static const char *copyVFatName(const char *ptr, void *dest, int len)
{
    uint8_t *dst = (uint8_t *)dest;

    for (int i = 0; i < len; ++i)
    {
        if (ptr == NULL)
        {
            *dst++ = 0xff;
            *dst++ = 0xff;
        }
        else
        {
            *dst++ = *ptr;
            *dst++ = 0;
            if (*ptr)
                ptr++;
            else
                ptr = NULL;
        }
    }

    return ptr;
}

void MSCUF2::readDirData(uint8_t *dest, UF2FileEntry *dirdata, int blkno)
{
    DirEntry *d = (DirEntry *)dest;
    int idx = blkno * -16;

    if (idx++ == 0)
    {
        paddedMemcpy(d->name, volumeLabel(), 11);
        d->attrs = 0x28;
        d++;
    }

    for (auto e = dirdata; e; e = e->next)
    {
        if (idx >= 16)
            break;

        char fatname[8 + 3 + 1];

        paddedMemcpy(fatname, e->filename, 8);
        auto dot = strchr(e->filename, '.');
        if (dot)
            paddedMemcpy(fatname + 8, dot + 1, 3);

        {
            char buf[20];
            itoa(e->id, buf);
            int idlen = strlen(buf);
            fatname[8 - idlen - 1] = '~';
            memcpy(fatname + 8 - idlen, buf, idlen);
        }

        LOG("list: %s [%s] sz:%d st:%d", e->filename, fatname, e->size, e->startCluster);

        int numdirentries = numDirEntries(e);
        for (int i = 0; i < numdirentries; ++i, ++idx)
        {
            if (0 <= idx && idx < 16)
            {
                if (i == numdirentries - 1)
                {
                    memcpy(d->name, fatname, sizeof(d->name));
                    d->attrs = e->attrs;
                    d->size = e->size;
                    d->startCluster = e->startCluster;
                    // timeToFat(e->mtime, &d->updateDate, &d->updateTime);
                    // timeToFat(e->ctime, &d->createDate, &d->createTime);
                }
                else
                {
                    VFatEntry *f = (VFatEntry *)d;
                    int seq = numdirentries - i - 2;
                    f->seqno = seq + 1; // they start at 1
                    if (i == 0)
                        f->seqno |= 0x40;
                    f->attrs = 0x0F;
                    f->type = 0x00;
                    f->checksum = fatChecksum(fatname);
                    f->startCluster = 0;

                    const char *ptr = e->filename + (13 * seq);
                    ptr = copyVFatName(ptr, f->name0, 5);
                    ptr = copyVFatName(ptr, f->name1, 6);
                    ptr = copyVFatName(ptr, f->name2, 2);
                }
                d++;
            }
        }
    }
}

#define WRITE_ENT(v)                                                                               \
    do                                                                                             \
    {                                                                                              \
        if (skip++ >= 0)                                                                           \
            *dest++ = v;                                                                           \
        if (skip >= 256)                                                                           \
            return;                                                                                \
        cl++;                                                                                      \
    } while (0)

void MSCUF2::buildBlock(uint32_t block_no, uint8_t *data)
{
    memset(data, 0, 512);
    uint32_t sectionIdx = block_no;

    if (block_no == 0)
    {
        buildFATBootBlock(data, volumeLabel(), NUM_FAT_BLOCKS);
    }
    else if (block_no < START_ROOTDIR)
    {
        sectionIdx -= START_FAT0;
        // logval("sidx", sectionIdx);
        if (sectionIdx >= SECTORS_PER_FAT)
            sectionIdx -= SECTORS_PER_FAT;

        int cl = 0;
        int skip = -(sectionIdx * 256);
        auto dest = (uint16_t *)data;

        WRITE_ENT(0xfff0);
        WRITE_ENT(0xffff);
        for (auto p = files; p; p = p->next)
        {
            int n = numClusters(p) - 1;
            for (int i = 0; i < n; i++)
                WRITE_ENT(cl + 1);
            WRITE_ENT(0xffff);
        }
    }
    else if (block_no < START_CLUSTERS)
    {
        sectionIdx -= START_ROOTDIR;
        readDirData(data, files, sectionIdx);
    }
    else
    {
        sectionIdx -= START_CLUSTERS;
        for (auto p = files; p; p = p->next)
        {
            if (p->startCluster <= sectionIdx && (int)sectionIdx < p->startCluster + numClusters(p))
            {
                readFileBlock(p->id, sectionIdx - p->startCluster, (char *)data);
                break;
            }
        }
    }
}

void MSCUF2::readBlocks(int blockAddr, int numBlocks)
{
    uint8_t buf[512];

    while (numBlocks--)
    {
        buildBlock(blockAddr, buf);
        writeBulk(buf, 512);
        blockAddr++;
    }

    finishReadWrite();
}

void MSCUF2::writeBlocks(int blockAddr, int numBlocks)
{
    uint8_t buf[512];

    while (numBlocks--)
    {
        readBulk(buf, sizeof(buf));
        if (is_uf2_block(buf))
        {
            UF2_Block *b = (UF2_Block *)buf;
            if (!(b->flags & UF2_FLAG_NOFLASH))
            {
                check_uf2_handover(buf, numBlocks, in->ep & 0xf, out->ep & 0xf, cbwTag());
            }
        }
        blockAddr++;
    }

    finishReadWrite();
}

MSCUF2::MSCUF2()
{
    files = NULL;
}

void MSCUF2::addFile(uint16_t id, const char *filename, uint32_t size)
{
    auto f = (UF2FileEntry *)malloc(sizeof(UF2FileEntry) + strlen(filename) + 1);
    memset(f, 0, sizeof(UF2FileEntry));
    strcpy(f->filename, filename);
    f->size = size;
    f->id = id;
    if (files == NULL)
        files = f;
    else
    {
        auto p = files;
        while (p->next)
            p = p->next;
        p->next = f;
        f->startCluster = p->startCluster + numClusters(p);
    }
}

static const char *index_htm = "<HTML>";

void MSCUF2::addFiles()
{
    addFile(1, "info_uf2.txt", strlen(uf2_info()));
    addFile(2, "index.htm", strlen(index_htm));
    addFile(3, "current.uf2", UF2_SIZE);
#if DEVICE_DMESG_BUFFER_SIZE > 0
    addFile(4, "dmesg.txt", DEVICE_DMESG_BUFFER_SIZE);
#endif
}

void MSCUF2::readFileBlock(uint16_t id, int blockAddr, char *dst)
{
    uint32_t addr;

    switch (id)
    {
    case 1:
        strcpy(dst, uf2_info());
        break;
    case 2:
        strcpy(dst, index_htm);
        break;
    case 3:
        addr = blockAddr * 256;
        if (addr < flashSize())
        {
            UF2_Block *bl = (UF2_Block *)dst;
            bl->magicStart0 = UF2_MAGIC_START0;
            bl->magicStart1 = UF2_MAGIC_START1;
            bl->magicEnd = UF2_MAGIC_END;
            bl->blockNo = blockAddr;
            bl->numBlocks = flashSize() / 256;
            bl->targetAddr = addr;
            bl->payloadSize = 256;
            memcpy(bl->data, (void *)addr, bl->payloadSize);
        }
        break;
#if DEVICE_DMESG_BUFFER_SIZE > 0
    case 4:
        addr = blockAddr * 512;
        for (uint32_t i = 0; i < 512; ++i)
        {
            if (addr < codalLogStore.ptr)
                *dst++ = codalLogStore.buffer[addr++];
            else
                *dst++ = '\n';
        }
        break;
#endif
    }
}
}

#endif

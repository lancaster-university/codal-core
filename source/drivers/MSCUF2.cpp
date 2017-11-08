#include "MSCUF2.h"
#include "FAT.h"

#if CONFIG_ENABLED(DEVICE_USB)

#define UF2_DEFINE_HANDOVER 1
#include "uf2format.h"

#define NUM_FAT_BLOCKS 8000

#define UF2_SIZE (flashSize() * 2)
#define UF2_SECTORS (UF2_SIZE / 512)
#define UF2_FIRST_SECTOR (numTextFiles() + 2)
#define UF2_LAST_SECTOR (UF2_FIRST_SECTOR + UF2_SECTORS - 1)

#define SECTORS_PER_FAT FAT_SECTORS_PER_FAT(NUM_FAT_BLOCKS)
#define START_FAT0 FAT_START_FAT0(NUM_FAT_BLOCKS)
#define START_FAT1 FAT_START_FAT1(NUM_FAT_BLOCKS)
#define START_ROOTDIR FAT_START_ROOTDIR(NUM_FAT_BLOCKS)
#define START_CLUSTERS FAT_START_CLUSTERS(NUM_FAT_BLOCKS)

namespace codal
{

const char *MSCUF2::textFileName(int id)
{
    switch (id)
    {
    case 0:
        return "INFO_UF2TXT";
    case 1:
        return "INDEX   HTM";
    }
    return NULL;
}

const char *MSCUF2::textFileContent(int id)
{
    switch (id)
    {
    case 0:
        return uf2_info();
    case 1:
        return "<HTML>";
    }
    return NULL;
}

uint32_t MSCUF2::getCapacity()
{
    return NUM_FAT_BLOCKS;
}

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
        if (sectionIdx == 0)
        {
            data[0] = 0xf0;
            for (unsigned i = 1; i < numTextFiles() * 2 + 4; ++i)
            {
                data[i] = 0xff;
            }
        }
        for (int i = 0; i < 256; ++i)
        {
            uint32_t v = sectionIdx * 256 + i;
            if (UF2_FIRST_SECTOR <= v && v <= UF2_LAST_SECTOR)
                ((uint16_t *)(void *)data)[i] = v == UF2_LAST_SECTOR ? 0xffff : v + 1;
        }
    }
    else if (block_no < START_CLUSTERS)
    {
        sectionIdx -= START_ROOTDIR;
        if (sectionIdx == 0)
        {
            DirEntry *d = (DirEntry *)data;

            fillFATDirEntry(d, volumeLabel(), 0, 0);
            d->attrs = 0x28;
            d++;

            unsigned i;
            for (i = 0; i < numTextFiles(); ++i)
                fillFATDirEntry(d++, textFileName(i), strlen(textFileContent(i)), i + 2);

            fillFATDirEntry(d++, "CURRENT UF2", UF2_SIZE, i + 2);
        }
    }
    else
    {
        sectionIdx -= START_CLUSTERS;
        if (sectionIdx < numTextFiles())
        {
            strcpy((char *)data, textFileContent(sectionIdx));
        }
        else
        {
            sectionIdx -= numTextFiles();
            uint32_t addr = sectionIdx * 256;
            if (addr < flashSize())
            {
                UF2_Block *bl = (UF2_Block *)data;
                bl->magicStart0 = UF2_MAGIC_START0;
                bl->magicStart1 = UF2_MAGIC_START1;
                bl->magicEnd = UF2_MAGIC_END;
                bl->blockNo = sectionIdx;
                bl->numBlocks = flashSize() / 256;
                bl->targetAddr = addr;
                bl->payloadSize = 256;
                memcpy(bl->data, (void *)addr, bl->payloadSize);
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
}


}

#endif

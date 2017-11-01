#include "MSCUF2.h"

#if CONFIG_ENABLED(DEVICE_USB)

#define UF2_DEFINE_HANDOVER 1
#include "uf2format.h"

#define FLASH_SIZE (256 * 1024)
#define VOLUME_LABEL "UF2BOOT"

namespace codal
{

typedef struct
{
    uint8_t JumpInstruction[3];
    uint8_t OEMInfo[8];
    uint16_t SectorSize;
    uint8_t SectorsPerCluster;
    uint16_t ReservedSectors;
    uint8_t FATCopies;
    uint16_t RootDirectoryEntries;
    uint16_t TotalSectors16;
    uint8_t MediaDescriptor;
    uint16_t SectorsPerFAT;
    uint16_t SectorsPerTrack;
    uint16_t Heads;
    uint32_t HiddenSectors;
    uint32_t TotalSectors32;
    uint8_t PhysicalDriveNum;
    uint8_t Reserved;
    uint8_t ExtendedBootSig;
    uint32_t VolumeSerialNumber;
    char VolumeLabel[11];
    uint8_t FilesystemIdentifier[8];
} __attribute__((packed)) FAT_BootBlock;

typedef struct
{
    char name[8];
    char ext[3];
    uint8_t attrs;
    uint8_t reserved;
    uint8_t createTimeFine;
    uint16_t createTime;
    uint16_t createDate;
    uint16_t lastAccessDate;
    uint16_t highStartCluster;
    uint16_t updateTime;
    uint16_t updateDate;
    uint16_t startCluster;
    uint32_t size;
} __attribute__((packed)) DirEntry;

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

#define UF2_SIZE (FLASH_SIZE * 2)
#define UF2_SECTORS (UF2_SIZE / 512)
#define UF2_FIRST_SECTOR (numTextFiles() + 1)
#define UF2_LAST_SECTOR (UF2_FIRST_SECTOR + UF2_SECTORS - 1)

#define RESERVED_SECTORS 1
#define ROOT_DIR_SECTORS 4
#define SECTORS_PER_FAT ((NUM_FAT_BLOCKS * 2 + 511) / 512)

#define START_FAT0 RESERVED_SECTORS
#define START_FAT1 (START_FAT0 + SECTORS_PER_FAT)
#define START_ROOTDIR (START_FAT1 + SECTORS_PER_FAT)
#define START_CLUSTERS (START_ROOTDIR + ROOT_DIR_SECTORS)

#define NUM_FAT_BLOCKS 8000

static const FAT_BootBlock BootBlock = {
    {0xeb, 0x3c, 0x90},                       // JumpInstruction
    {'U', 'F', '2', ' ', 'U', 'F', '2', ' '}, // OEMInfo
    512,                                      // SectorSize
    1,                                        // SectorsPerCluster
    RESERVED_SECTORS,                         // ReservedSectors
    2,                                        // FATCopies
    (ROOT_DIR_SECTORS * 512 / 32),            // RootDirectoryEntries
    NUM_FAT_BLOCKS - 2,                       // TotalSectors16
    0xF8,                                     // MediaDescriptor
    SECTORS_PER_FAT,                          // SectorsPerFAT
    1,                                        // SectorsPerTrack
    1,                                        // Heads
    0,                                        // HiddenSectors
    0,                                        // TotalSectors32
    0,                                        // PhysicalDriveNum
    0,                                        // Reserved
    0x29,                                     // ExtendedBootSig
    0x00420042,                               // VolumeSerialNumber
    VOLUME_LABEL,                             // VolumeLabel
    {'F', 'A', 'T', '1', '6', ' ', ' ', ' '}, // FilesystemIdentifier
};

void padded_memcpy(char *dst, const char *src, int len)
{
    for (int i = 0; i < len; ++i)
    {
        if (*src)
            *dst = *src++;
        else
            *dst = ' ';
        dst++;
    }
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
        memcpy(data, &BootBlock, sizeof(BootBlock));
        data[510] = 0x55;
        data[511] = 0xaa;
        // logval("data[0]", data[0]);
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
            unsigned i;
            padded_memcpy(d->name, BootBlock.VolumeLabel, 11);
            d->attrs = 0x28;
            for (i = 0; i < numTextFiles(); ++i)
            {
                d++;
                d->size = strlen(textFileContent(i));
                d->startCluster = i + 2;
                padded_memcpy(d->name, textFileName(i), 11);
            }

            d++;
            d->size = UF2_SIZE;
            d->startCluster = i + 2;
            padded_memcpy(d->name, "CURRENT UF2", 11);
        }
    }
    else
    {
        sectionIdx -= START_CLUSTERS;
        if (sectionIdx < numTextFiles() - 1)
        {
            strcpy((char*)data, textFileContent(sectionIdx));
        }
        else
        {
            sectionIdx -= numTextFiles() - 1;
            uint32_t addr = sectionIdx * 256;
            if (addr < FLASH_SIZE)
            {
                UF2_Block *bl = (UF2_Block *)data;
                bl->magicStart0 = UF2_MAGIC_START0;
                bl->magicStart1 = UF2_MAGIC_START1;
                bl->magicEnd = UF2_MAGIC_END;
                bl->blockNo = sectionIdx;
                bl->numBlocks = FLASH_SIZE / 256;
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
}

#endif

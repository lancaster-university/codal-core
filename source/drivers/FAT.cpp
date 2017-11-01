#include "FAT.h"
#include <string.h>

namespace codal
{

static const FAT_BootBlock BootBlock = {
    {0xeb, 0x3c, 0x90},                       // JumpInstruction
    {'C', 'O', 'D', 'A', 'L', ' ', '0', '0'}, // OEMInfo
    512,                                      // SectorSize
    1,                                        // SectorsPerCluster
    FAT_RESERVED_SECTORS,                     // ReservedSectors
    2,                                        // FATCopies
    (FAT_ROOT_DIR_SECTORS * 512 / 32),        // RootDirectoryEntries
    0,                                        // TotalSectors16
    0xF8,                                     // MediaDescriptor
    0,                                        // SectorsPerFAT
    1,                                        // SectorsPerTrack
    1,                                        // Heads
    0,                                        // HiddenSectors
    0,                                        // TotalSectors32
    0,                                        // PhysicalDriveNum
    0,                                        // Reserved
    0x29,                                     // ExtendedBootSig
    0x00420042,                               // VolumeSerialNumber
    "",                                       // VolumeLabel
    {'F', 'A', 'T', '1', '6', ' ', ' ', ' '}, // FilesystemIdentifier
};

static void paddedMemcpy(char *dst, const char *src, int len)
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

void buildFATBootBlock(uint8_t *data, const char *volumeLabel, uint16_t numFsBlocks)
{
    memset(data, 0, 512);
    memcpy(data, &BootBlock, sizeof(BootBlock));
    FAT_BootBlock *bb = (FAT_BootBlock *)data;
    paddedMemcpy(bb->VolumeLabel, volumeLabel, 11);
    bb->TotalSectors16 = numFsBlocks - 2;
    bb->SectorsPerFAT = FAT_SECTORS_PER_FAT(numFsBlocks);
    data[510] = 0x55;
    data[511] = 0xaa;
}

void fillFATDirEntry(DirEntry *d, const char *filename, int size, int startCluster)
{
    d->size = size;
    d->startCluster = startCluster;
    paddedMemcpy(d->name, filename, 11);
}

void buildEmptyFAT(uint8_t *data, uint32_t blockNo, const char *volumeLabel, uint16_t numFsBlocks)
{
    uint32_t sectionIdx = blockNo;

    memset(data, 0, 512);

    if (blockNo == 0)
    {
        buildFATBootBlock(data, volumeLabel, numFsBlocks);
    }
    else if (blockNo < FAT_START_ROOTDIR(numFsBlocks))
    {
        sectionIdx -= FAT_START_FAT0(numFsBlocks);
        if (sectionIdx >= FAT_SECTORS_PER_FAT(numFsBlocks))
            sectionIdx -= FAT_SECTORS_PER_FAT(numFsBlocks);
        if (sectionIdx == 0)
        {
            data[0] = 0xf0;
            for (unsigned i = 1; i < 4; ++i)
            {
                data[i] = 0xff;
            }
        }
    }
    else if (blockNo < FAT_START_CLUSTERS(numFsBlocks))
    {
        sectionIdx -= FAT_START_ROOTDIR(numFsBlocks);
        if (sectionIdx == 0)
        {
            DirEntry *d = (DirEntry *)data;
            paddedMemcpy(d->name, volumeLabel, 11);
            d->attrs = 0x28;
        }
    }
    else
    {
        // leave as zero
    }
}
}

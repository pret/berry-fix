#ifndef GUARD_SAVE_H
#define GUARD_SAVE_H

// Each 4 KiB flash sector contains 3968 bytes of actual data followed by a 128 byte footer.
// Only 12 bytes of the footer are used.
#define SECTOR_DATA_SIZE 3968
#define SECTOR_FOOTER_SIZE 128
#define SECTOR_SIZE (SECTOR_DATA_SIZE + SECTOR_FOOTER_SIZE)

#define NUM_SAVE_SLOTS 2

// If the sector's security field is not this value then the sector is either invalid or empty.
#define SECTOR_SECURITY_NUM 0x8012025

#define SECTOR_ID_SAVEBLOCK2          0
#define SECTOR_ID_SAVEBLOCK1_START    1
#define SECTOR_ID_SAVEBLOCK1_END      4
#define NUM_SECTORS_PER_SLOT         14
#define SECTORS_COUNT                32

#define SAVE_STATUS_EMPTY    0
#define SAVE_STATUS_OK       1
#define SAVE_STATUS_CORRUPT  2
#define SAVE_STATUS_NO_FLASH 4
#define SAVE_STATUS_ERROR    0xFF

// Special sector id value for certain save functions to
// indicate that no specific sector should be used.
#define FULL_SAVE_SLOT 0xFFFF

enum
{
    SECTOR_DAMAGED,
    SECTOR_OK,
    SECTOR_CHECK, // unused
};

struct SaveBlockChunk
{
    u8 * data;
    u16 size;
};

struct SaveSector
{
    u8 data[SECTOR_DATA_SIZE];
    u8 unused[SECTOR_FOOTER_SIZE - 12]; // Unused portion of the footer
    u16 id;
    u16 checksum;
    u32 security;
    u32 counter;
}; // size is SECTOR_SIZE (0x1000)

#define SECTOR_SECURITY_OFFSET offsetof(struct SaveSector, security)

extern u32 gDamagedSaveSectors;
extern bool32 gFlashIdentIsValid;
extern const struct SaveBlockChunk gSaveBlockChunks[];

u8 WriteSaveSectorOrSlot(u16 sectorId, const struct SaveBlockChunk *chunks);
u8 TryLoadSaveSlot(u16 sectorId, const struct SaveBlockChunk *chunks);

#endif //GUARD_SAVE_H

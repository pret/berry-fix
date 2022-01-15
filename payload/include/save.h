#ifndef GUARD_FLASH_H
#define GUARD_FLASH_H

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
#define SECTOR_ID_PKMN_STORAGE_START  5
#define SECTOR_ID_PKMN_STORAGE_END   13
#define NUM_SECTORS_PER_SLOT         14
// Save Slot 1: 0-13;  Save Slot 2: 14-27
#define SECTOR_ID_HOF_1              28
#define SECTOR_ID_HOF_2              29
#define SECTOR_ID_TRAINER_HILL       30
#define SECTOR_ID_RECORDED_BATTLE    31
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

enum
{
    SAVE_NORMAL,     // Save full save slot
    SAVE_SAVEBLOCKS, // Save just SaveBlock1 and SaveBlock2
    SAVE_SAVEBLOCK2, // Save just SaveBlock2
};

enum MsgBoxUpdateMessage
{
    MSGBOX_WILL_NOW_UPDATE = 0,
    MSGBOX_HAS_BEEN_UPDATED,
    MSGBOX_UNABLE_TO_UPDATE,
    MSGBOX_NO_NEED_TO_UPDATE,
    MSGBOX_UPDATING
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

bool32 BerryFix_IdentifyFlash(void);
bool8 BerryFix_LoadSave(u32);
void msg_load_gfx(void);
void msg_display(enum MsgBoxUpdateMessage);
bool32 flash_maincb_check_need_reset_pacifidlog_tm(void);
bool32 flash_maincb_reset_pacifidlog_tm(void);

#endif //GUARD_FLASH_H

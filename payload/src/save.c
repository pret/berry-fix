#include "global.h"
#include "main.h"
#include "save.h"
#include "rtc.h"

/*
    The Berry Fix Program contains a copy of most of Ruby/Sapphire's save code.
    Much of it lies unused.
*/

struct SaveBlockChunk
{
    u8 * data;
    u16 size;
};

static u8 WriteSaveSectorOrSlot(u16, const struct SaveBlockChunk *);
static u8 HandleWriteSector(u16, const struct SaveBlockChunk *);
static u8 TryWriteSector(u8, u8 *);
static u8 HandleReplaceSector(u16, const struct SaveBlockChunk *);
static u8 TryLoadSaveSlot(u16, const struct SaveBlockChunk *);
static u8 CopySaveSlotData(u16, const struct SaveBlockChunk *);
static u8 GetSaveValidStatus(const struct SaveBlockChunk *);
static u32 ReadFlashSector(u8, struct SaveSector *);
static u16 CalculateChecksum(const void *, u16);

u16 gLastWrittenSector;
u32 gLastSaveCounter;
u16 gLastKnownGoodSector;
u32 gDamagedSaveSectors;
u32 gSaveCounter;
struct SaveSector * gReadWriteSector;
u16 gIncrementalSectorId;
bool32 gFlashIdentIsValid;

#define gSaveDataBuffer ((struct SaveSector *)(EWRAM_START + 0x20000))

EWRAM_DATA struct SaveBlock2 gSaveBlock2 = {};
EWRAM_DATA struct SaveBlock1 gSaveBlock1 = {};
EWRAM_DATA struct PokemonStorage gPokemonStorage = {};

#define SAVEBLOCK_CHUNK(structure, chunkNum)                                \
{                                                                           \
    (u8 *)&structure + chunkNum * SECTOR_DATA_SIZE,                         \
    min(sizeof(structure) - chunkNum * SECTOR_DATA_SIZE, SECTOR_DATA_SIZE)  \
}                                                                           \

static const struct SaveBlockChunk sSaveBlockChunks[] =
{
    SAVEBLOCK_CHUNK(gSaveBlock2, 0),

    SAVEBLOCK_CHUNK(gSaveBlock1, 0),
    SAVEBLOCK_CHUNK(gSaveBlock1, 1),
    SAVEBLOCK_CHUNK(gSaveBlock1, 2),
    SAVEBLOCK_CHUNK(gSaveBlock1, 3),

    SAVEBLOCK_CHUNK(gPokemonStorage, 0),
    SAVEBLOCK_CHUNK(gPokemonStorage, 1),
    SAVEBLOCK_CHUNK(gPokemonStorage, 2),
    SAVEBLOCK_CHUNK(gPokemonStorage, 3),
    SAVEBLOCK_CHUNK(gPokemonStorage, 4),
    SAVEBLOCK_CHUNK(gPokemonStorage, 5),
    SAVEBLOCK_CHUNK(gPokemonStorage, 6),
    SAVEBLOCK_CHUNK(gPokemonStorage, 7),
    SAVEBLOCK_CHUNK(gPokemonStorage, 8),
};

const u16 gInfoMessagesPal[] = INCBIN_U16("graphics/msg_box.gbapal");
const u8 gInfoMessagesTilemap[] = INCBIN_U8("graphics/msg_box.tilemap.lz");
const u8 gInfoMessagesGfx[] = INCBIN_U8("graphics/msg_box.4bpp.lz");

bool32 BerryFix_IdentifyFlash(void)
{
    gFlashIdentIsValid = TRUE;
    if (!IdentifyFlash())
    {
        SetFlashTimerIntr(0, &((IntrFunc *)gIntrFuncPointers)[9]);
        return TRUE;
    }
    gFlashIdentIsValid = FALSE;
    return FALSE;
}

// Unused
static void BerryFix_ReadFlash(u16 sectorNum, ptrdiff_t offset, void * dest, size_t size)
{
    ReadFlash(sectorNum, offset, dest, size);
}

static u8 BerryFix_WriteSaveSectorOrSlot(u16 sectorId, const struct SaveBlockChunk * chunks)
{
    return WriteSaveSectorOrSlot(sectorId, chunks);
}

static u8 BerryFix_TryLoadSaveSlot(u16 sectorId, const struct SaveBlockChunk * chunks)
{
    return TryLoadSaveSlot(sectorId, chunks);
}

static u32 * BerryFix_GetDamagedSaveSectors(void)
{
    return &gDamagedSaveSectors;
}

static s32 BerryFix_Save(u8 mode)
{
    u8 i;
    switch (mode)
    {
    case SAVE_NORMAL:
    default:
        BerryFix_WriteSaveSectorOrSlot(FULL_SAVE_SLOT, sSaveBlockChunks);
        break;
    case SAVE_SAVEBLOCKS:
        for (i = SECTOR_ID_SAVEBLOCK2; i <= SECTOR_ID_SAVEBLOCK1_END; i++)
            BerryFix_WriteSaveSectorOrSlot(i, sSaveBlockChunks);
        break;
    case SAVE_SAVEBLOCK2:
        BerryFix_WriteSaveSectorOrSlot(SECTOR_ID_SAVEBLOCK2, sSaveBlockChunks);
        break;
    }

    return 0;
}

u8 BerryFix_TrySave(u8 mode)
{
    BerryFix_Save(mode);
    if (*BerryFix_GetDamagedSaveSectors() == 0)
        return SAVE_STATUS_OK;
    return SAVE_STATUS_ERROR;
}

u8 BerryFix_LoadSave(u32 unused)
{
    return BerryFix_TryLoadSaveSlot(FULL_SAVE_SLOT, sSaveBlockChunks);
}

void msg_load_gfx(void)
{
    REG_DISPCNT = 0;
    REG_BG0HOFS = 0;
    REG_BG0VOFS = 0;
    REG_BLDCNT = 0;
    LZ77UnCompVram(gInfoMessagesGfx, (void *)BG_VRAM);
    LZ77UnCompVram(gInfoMessagesTilemap, (void *)BG_SCREEN_ADDR(28));
    CpuCopy16(gInfoMessagesPal, (void *)BG_PLTT, BG_PLTT_SIZE);
    REG_BG0CNT = BGCNT_SCREENBASE(28) | BGCNT_TXT512x512;
    REG_DISPCNT = DISPCNT_BG0_ON;
}

void msg_display(enum MsgBoxUpdateMessage a0)
{
    switch (a0)
    {
        case MSGBOX_WILL_NOW_UPDATE:
            REG_BG0HOFS = 0;
            REG_BG0VOFS = 0;
            break;
        case MSGBOX_HAS_BEEN_UPDATED:
            REG_BG0HOFS = 0x100;
            REG_BG0VOFS = 0;
            break;
        case MSGBOX_UNABLE_TO_UPDATE:
            REG_BG0HOFS = 0x100;
            REG_BG0VOFS = 0xB0;
            break;
        case MSGBOX_NO_NEED_TO_UPDATE:
            REG_BG0HOFS = 0;
            REG_BG0VOFS = 0xB0;
            break;
        case MSGBOX_UPDATING:
            REG_BG0HOFS = 0;
            REG_BG0VOFS = 0x160;
            break;
    }
}

// Unused
static void ClearSaveData(void)
{
    u16 i;
    for (i = 0; i < SECTORS_COUNT; i++)
        EraseFlashSector(i);
}

// Unused
static void Save_ResetSaveCounters(void)
{
    gSaveCounter = 0;
    gLastWrittenSector = 0;
    gDamagedSaveSectors = 0;
}

static bool32 SetDamagedSectorBits(u8 op, u8 sectorId)
{
    bool32 retVal = FALSE;

    switch (op)
    {
    case SECTOR_DAMAGED:
        gDamagedSaveSectors |= (1 << sectorId);
        break;
    case SECTOR_OK:
        gDamagedSaveSectors &= ~(1 << sectorId);
        break;
    case SECTOR_CHECK: // unused
        if (gDamagedSaveSectors & (1 << sectorId))
            retVal = TRUE;
        break;
    }

    return retVal;
}

static u8 WriteSaveSectorOrSlot(u16 sectorId, const struct SaveBlockChunk *chunks)
{
    u32 status;
    u16 i;

    gReadWriteSector = gSaveDataBuffer;

    if (sectorId != FULL_SAVE_SLOT)
    {
        // A sector was specified, just write that sector.
        status = HandleWriteSector(sectorId, chunks);
    }
    else
    {
        // No sector was specified, write full save slot.
        gLastKnownGoodSector = gLastWrittenSector;
        gLastSaveCounter = gSaveCounter;
        gLastWrittenSector++;
        gLastWrittenSector %= NUM_SECTORS_PER_SLOT;
        gSaveCounter++;
        status = SAVE_STATUS_OK;

        for (i = 0; i < NUM_SECTORS_PER_SLOT; i++)
            HandleWriteSector(i, chunks);

        if (gDamagedSaveSectors)
        {
            // At least one sector save failed
            status = SAVE_STATUS_ERROR;
            gLastWrittenSector = gLastKnownGoodSector;
            gSaveCounter = gLastSaveCounter;
        }
    }

    return status;
}

static u8 HandleWriteSector(u16 sectorId, const struct SaveBlockChunk * chunks)
{
    u16 i;
    u16 sectorNum;
    u8 *data;
    u16 size;

    // Adjust sector id for current save slot
    sectorNum = sectorId + gLastWrittenSector;
    sectorNum %= NUM_SECTORS_PER_SLOT;
    sectorNum += NUM_SECTORS_PER_SLOT * (gSaveCounter % NUM_SAVE_SLOTS);

    // Get current save data
    data = chunks[sectorId].data;
    size = chunks[sectorId].size;

    // Clear temp save sector
    for (i = 0; i < SECTOR_SIZE; i++)
        ((u8 *)gReadWriteSector)[i] = 0;

    // Set footer data
    gReadWriteSector->id = sectorId;
    gReadWriteSector->security = SECTOR_SECURITY_NUM;
    gReadWriteSector->counter = gSaveCounter;

    // Copy current data to temp buffer for writing
    for (i = 0; i < size; i++)
        gReadWriteSector->data[i] = data[i];

    gReadWriteSector->checksum = CalculateChecksum(data, size);

    return TryWriteSector(sectorNum, gReadWriteSector->data);
}

// Unused
static u8 HandleWriteSectorNBytes(u8 sectorId, u8 *data, u16 size)
{
    u16 i;
    struct SaveSector *sector = gSaveDataBuffer;

    // Clear temp save sector
    for (i = 0; i < SECTOR_SIZE; i++)
        ((u8 *)sector)[i] = 0;

    sector->security = SECTOR_SECURITY_NUM;

    // Copy data to temp buffer for writing
    for (i = 0; i < size; i++)
        sector->data[i] = data[i];

    sector->id = CalculateChecksum(data, size); // though this appears to be incorrect, it might be some sector checksum instead of a whole save checksum and only appears to be relevent to HOF data, if used.
    return TryWriteSector(sectorId, sector->data);
}

static u8 TryWriteSector(u8 sectorNum, u8 *data)
{
    if (ProgramFlashSectorAndVerify(sectorNum, data) != 0) // is damaged?
    {
        // Failed
        SetDamagedSectorBits(SECTOR_DAMAGED, sectorNum);
        return SAVE_STATUS_ERROR;
    }
    else
    {
        // Succeeded
        SetDamagedSectorBits(SECTOR_OK, sectorNum);
        return SAVE_STATUS_OK;
    }
}

// Unused
static u32 RestoreSaveBackupVarsAndIncrement(const struct SaveBlockChunk *chunks)
{
    gReadWriteSector = gSaveDataBuffer;
    gLastKnownGoodSector = gLastWrittenSector;
    gLastSaveCounter = gSaveCounter;
    gLastWrittenSector++;
    gLastWrittenSector %= NUM_SECTORS_PER_SLOT;
    gSaveCounter++;
    gIncrementalSectorId = 0;
    gDamagedSaveSectors = 0;
    return 0;
}

// Unused
static u32 RestoreSaveBackupVars(const struct SaveBlockChunk *chunks)
{
    gReadWriteSector = gSaveDataBuffer;
    gLastKnownGoodSector = gLastWrittenSector;
    gLastSaveCounter = gSaveCounter;
    gIncrementalSectorId = 0;
    gDamagedSaveSectors = 0;
    return 0;
}

// Unused
static u8 HandleWriteIncrementalSector(u16 numSectors, const struct SaveBlockChunk * chunks)
{
    u8 status;

    if (gIncrementalSectorId < numSectors - 1)
    {
        status = SAVE_STATUS_OK;
        HandleWriteSector(gIncrementalSectorId, chunks);
        gIncrementalSectorId++;
        if (gDamagedSaveSectors)
        {
            status = SAVE_STATUS_ERROR;
            gLastWrittenSector = gLastKnownGoodSector;
            gSaveCounter = gLastSaveCounter;
        }
    }
    else
    {
        // Exceeded max sector, finished
        status = SAVE_STATUS_ERROR;
    }

    return status;
}

// Unused
static u8 HandleReplaceSectorAndVerify(u16 sectorId, const struct SaveBlockChunk *chunks)
{
    u8 status = SAVE_STATUS_OK;

    HandleReplaceSector(sectorId - 1, chunks);

    if (gDamagedSaveSectors)
    {
        status = SAVE_STATUS_ERROR;
        gLastWrittenSector = gLastKnownGoodSector;
        gSaveCounter = gLastSaveCounter;
    }
    return status;
}

// Unused
// Similar to HandleWriteSector, but fully erases the sector first, and skips writing the first security byte
static u8 HandleReplaceSector(u16 sectorId, const struct SaveBlockChunk *chunks)
{
    u16 i;
    u16 sector;
    u8 *data;
    u16 size;
    u8 status;

    // Adjust sector id for current save slot
    sector = sectorId + gLastWrittenSector;
    sector %= NUM_SECTORS_PER_SLOT;
    sector += NUM_SECTORS_PER_SLOT * (gSaveCounter % NUM_SAVE_SLOTS);

    // Get current save data
    data = chunks[sectorId].data;
    size = chunks[sectorId].size;

    // Clear temp save sector.
    for (i = 0; i < SECTOR_SIZE; i++)
        ((u8 *)gReadWriteSector)[i] = 0;

    gReadWriteSector->id = sectorId;
    gReadWriteSector->security = SECTOR_SECURITY_NUM;
    gReadWriteSector->counter = gSaveCounter;

    // set temp section's data.
    for (i = 0; i < size; i++)
        gReadWriteSector->data[i] = data[i];

    // calculate checksum.
    gReadWriteSector->checksum = CalculateChecksum(data, size);

    EraseFlashSector(sector);

    status = SAVE_STATUS_OK;

    // Write new save data up to security field
    for (i = 0; i < SECTOR_SECURITY_OFFSET; i++)
    {
        if (ProgramFlashByte(sector, i, gReadWriteSector->data[i]))
        {
            status = SAVE_STATUS_ERROR;
            break;
        }
    }

    if (status == SAVE_STATUS_ERROR)
    {
        // Writing save data failed
        SetDamagedSectorBits(SECTOR_DAMAGED, sector);
        return SAVE_STATUS_ERROR;
    }
    else
    {
        // Writing save data succeeded, write security and counter
        status = SAVE_STATUS_OK;

        // Write security (skipping the first byte) and counter fields.
        // The byte of security that is skipped is instead written by WriteSectorSecurityByte or WriteSectorSecurityByte_NoOffset
        for (i = 0; i < SECTOR_SIZE - (SECTOR_SECURITY_OFFSET + 1); i++)
        {
            if (ProgramFlashByte(sector, SECTOR_SECURITY_OFFSET + 1 + i, ((u8 *)gReadWriteSector)[SECTOR_SECURITY_OFFSET + 1 + i]))
            {
                status = SAVE_STATUS_ERROR;
                break;
            }
        }

        if (status == SAVE_STATUS_ERROR)
        {
            // Writing security/counter failed
            SetDamagedSectorBits(SECTOR_DAMAGED, sector);
            return SAVE_STATUS_ERROR;
        }
        else
        {
            // Succeeded
            SetDamagedSectorBits(SECTOR_OK, sector);
            return SAVE_STATUS_OK;
        }
    }
}

// Unused
static u8 CopySectorSecurityByte(u16 sectorId, const struct SaveBlockChunk *chunks)
{
    // Adjust sector id for current save slot
    u16 sector = sectorId + gLastWrittenSector - 1;
    sector %= NUM_SECTORS_PER_SLOT;
    sector += NUM_SECTORS_PER_SLOT * (gSaveCounter % NUM_SAVE_SLOTS);

    // Copy just the first byte of the security field from the read/write buffer
    if (ProgramFlashByte(sector, SECTOR_SECURITY_OFFSET, ((u8 *)gReadWriteSector)[SECTOR_SECURITY_OFFSET]))
    {
        // Sector is damaged, so enable the bit in gDamagedSaveSectors and restore the last written sector and save counter.
        SetDamagedSectorBits(SECTOR_DAMAGED, sector);
        gLastWrittenSector = gLastKnownGoodSector;
        gSaveCounter = gLastSaveCounter;
        return SAVE_STATUS_ERROR;
    }
    else
    {
        SetDamagedSectorBits(SECTOR_OK, sector);
        return SAVE_STATUS_OK;
    }
}

// Unused
static u8 WriteSectorSecurityByte(u16 sectorId, const struct SaveBlockChunk *chunks)
{
    // Adjust sector id for current save slot
    u16 sector = sectorId + gLastWrittenSector - 1;
    sector %= NUM_SECTORS_PER_SLOT;
    sector += NUM_SECTORS_PER_SLOT * (gSaveCounter % 2);

    // Write just the first byte of the security field, which was skipped by HandleReplaceSector
    if (ProgramFlashByte(sector, SECTOR_SECURITY_OFFSET, SECTOR_SECURITY_NUM & 0xFF))
    {
        // Sector is damaged, so enable the bit in gDamagedSaveSectors and restore the last written sector and save counter.
        SetDamagedSectorBits(SECTOR_DAMAGED, sector);
        gLastWrittenSector = gLastKnownGoodSector;
        gSaveCounter = gLastSaveCounter;
        return SAVE_STATUS_ERROR;
    }
    else
    {
        // Succeeded
        SetDamagedSectorBits(SECTOR_OK, sector);
        return SAVE_STATUS_OK;
    }
}

static u8 TryLoadSaveSlot(u16 sectorId, const struct SaveBlockChunk *chunks)
{
    u8 status;
    gReadWriteSector = gSaveDataBuffer;
    if (sectorId != FULL_SAVE_SLOT)
    {
        // This function may not be used with a specific sector id
        status = SAVE_STATUS_ERROR;
    }
    else
    {
        status = GetSaveValidStatus(chunks);
        CopySaveSlotData(FULL_SAVE_SLOT, chunks);
    }

    return status;
}

// sectorId arg is ignored, this always reads the full save slot
static u8 CopySaveSlotData(u16 sectorId, const struct SaveBlockChunk *chunks)
{
    u16 i;
    u16 checksum;
    u16 slotOffset = NUM_SECTORS_PER_SLOT * (gSaveCounter % NUM_SAVE_SLOTS);
    u16 id;

    for (i = 0; i < NUM_SECTORS_PER_SLOT; i++)
    {
        ReadFlashSector(i + slotOffset, gReadWriteSector);

        id = gReadWriteSector->id;
        if (id == 0)
            gLastWrittenSector = i;

        checksum = CalculateChecksum(gReadWriteSector->data, chunks[id].size);

        // Only copy data for sectors whose security and checksum fields are correct
        if (gReadWriteSector->security == SECTOR_SECURITY_NUM && gReadWriteSector->checksum == checksum)
        {
            u16 j;
            for (j = 0; j < chunks[id].size; j++)
                chunks[id].data[j] = gReadWriteSector->data[j];
        }
    }

    return SAVE_STATUS_OK;
}

static u8 GetSaveValidStatus(const struct SaveBlockChunk *chunks)
{
    u16 i;
    bool8 securityPassed;
    u16 checksum;
    u32 slot1saveCounter = 0;
    u32 slot2saveCounter = 0;
    u8 slot1Status;
    u8 slot2Status;
    u32 validSectors;
    const u32 ALL_SECTORS = (1 << NUM_SECTORS_PER_SLOT) - 1;  // bitmask of all saveblock sectors

    // Check save slot 1
    validSectors = 0;
    securityPassed = FALSE;
    for (i = 0; i < NUM_SECTORS_PER_SLOT; i++)
    {
        ReadFlashSector(i, gReadWriteSector);
        if (gReadWriteSector->security == SECTOR_SECURITY_NUM)
        {
            securityPassed = TRUE;
            checksum = CalculateChecksum(gReadWriteSector->data, chunks[gReadWriteSector->id].size);
            if (gReadWriteSector->checksum == checksum)
            {
                slot1saveCounter = gReadWriteSector->counter;
                validSectors |= 1 << gReadWriteSector->id;
            }
        }
    }

    if (securityPassed)
    {
        if (validSectors == ALL_SECTORS)
            slot1Status = SAVE_STATUS_OK;
        else
            slot1Status = SAVE_STATUS_ERROR;
    }
    else
    {
        // No sectors in slot 1 have the security number, treat it as empty
        slot1Status = SAVE_STATUS_EMPTY;
    }

    // Check save slot 2
    validSectors = 0;
    securityPassed = FALSE;
    for (i = 0; i < NUM_SECTORS_PER_SLOT; i++)
    {
        ReadFlashSector(NUM_SECTORS_PER_SLOT + i, gReadWriteSector);
        if (gReadWriteSector->security == SECTOR_SECURITY_NUM)
        {
            securityPassed = TRUE;
            checksum = CalculateChecksum(gReadWriteSector->data, chunks[gReadWriteSector->id].size);
            if (gReadWriteSector->checksum == checksum)
            {
                slot2saveCounter = gReadWriteSector->counter;
                validSectors |= 1 << gReadWriteSector->id;
            }
        }
    }

    if (securityPassed)
    {
        if (validSectors == ALL_SECTORS)
            slot2Status = SAVE_STATUS_OK;
        else
            slot2Status = SAVE_STATUS_ERROR;
    }
    else
    {
        // No sectors in slot 2 have the security number, treat it as empty.
        slot2Status = SAVE_STATUS_EMPTY;
    }

    if (slot1Status == SAVE_STATUS_OK && slot2Status == SAVE_STATUS_OK)
    {
        // Choose counter of the most recent save file
        if ((slot1saveCounter == -1 && slot2saveCounter ==  0)
         || (slot1saveCounter ==  0 && slot2saveCounter == -1))
        {
            if ((unsigned)(slot1saveCounter + 1) < (unsigned)(slot2saveCounter + 1))
                gSaveCounter = slot2saveCounter;
            else
                gSaveCounter = slot1saveCounter;
        }
        else
        {
            if (slot1saveCounter < slot2saveCounter)
                gSaveCounter = slot2saveCounter;
            else
                gSaveCounter = slot1saveCounter;
        }
        return SAVE_STATUS_OK;
    }

    // One or both save slots are not OK

    if (slot1Status == SAVE_STATUS_OK)
    {
        gSaveCounter = slot1saveCounter;
        if (slot2Status == SAVE_STATUS_ERROR)
            return SAVE_STATUS_ERROR; // Slot 2 errored
        else
            return SAVE_STATUS_OK; // Slot 1 is OK, slot 2 is empty
    }

    if (slot2Status == SAVE_STATUS_OK)
    {
        gSaveCounter = slot2saveCounter;
        if (slot1Status == SAVE_STATUS_ERROR)
            return SAVE_STATUS_ERROR; // Slot 1 errored
        else
            return SAVE_STATUS_OK; // Slot 2 is OK, slot 1 is empty
    }

    // Neither slot is OK, check if both are empty
    if (slot1Status == SAVE_STATUS_EMPTY 
     && slot2Status == SAVE_STATUS_EMPTY)
    {
        gSaveCounter = 0;
        gLastWrittenSector = 0;
        return SAVE_STATUS_EMPTY;
    }

    // Both slots errored
    gSaveCounter = 0;
    gLastWrittenSector = 0;
    return SAVE_STATUS_CORRUPT;
}

// Unused
static u8 TryLoadSaveSector(u8 sectorId, u8 *data, u16 size)
{
    u16 i;
    struct SaveSector *sector = gSaveDataBuffer;
    ReadFlashSector(sectorId, sector);
    if (sector->security == SECTOR_SECURITY_NUM)
    {
        u16 checksum = CalculateChecksum(sector->data, size);
        if (sector->id == checksum)
        {
            // Security and checksum are correct, copy data
            for (i = 0; i < size; i++)
                data[i] = sector->data[i];
            return SAVE_STATUS_OK;
        }
        else
        {
            // Incorrect checksum
            return SAVE_STATUS_CORRUPT;
        }
    }
    else
    {
        // Incorrect security value
        return SAVE_STATUS_EMPTY;
    }
}

// Return value always ignored
static u32 ReadFlashSector(u8 sectorId, struct SaveSector *sector)
{
    ReadFlash(sectorId, 0, sector->data, SECTOR_SIZE);
    return 1;
}

static u16 CalculateChecksum(const void *data, u16 size)
{
    u16 i;
    u32 checksum = 0;

    for (i = 0; i < (size / 4); i++)
    {
        checksum += *((u32 *)data);
        data += sizeof(u32);
    }

    return ((checksum >> 16) + checksum);
}

// Unused
static void SaveDummy1()
{
}

// Unused
static void SaveDummy2()
{
}

// Unused
static void SaveDummy3()
{
}

static u16 * GetVarPointer(u16 id)
{
    if (id < VARS_START)
        return NULL;
    if (id < SPECIAL_VARS_START)
        return &gSaveBlock1.vars[id - VARS_START];
    return NULL;
}

bool32 flash_maincb_check_need_reset_pacifidlog_tm(void)
{
    u8 sp0;
    u16 * data = GetVarPointer(VAR_PACIFIDLOG_TM_RECEIVED_DAY);
    rtc_maincb_is_time_since_last_berry_update_positive(&sp0);
    if (*data <= gRtcUTCTime.days)
        return TRUE;
    else
        return FALSE;
}

bool32 flash_maincb_reset_pacifidlog_tm(void)
{
    u8 sp0;
    if (flash_maincb_check_need_reset_pacifidlog_tm() == TRUE)
        return TRUE;
    rtc_maincb_is_time_since_last_berry_update_positive(&sp0);
    if (gRtcUTCTime.days < 0)
        return FALSE;
    *GetVarPointer(VAR_PACIFIDLOG_TM_RECEIVED_DAY) = 1;
    if (BerryFix_TrySave(SAVE_NORMAL) != SAVE_STATUS_OK)
        return FALSE;
    return TRUE;
}
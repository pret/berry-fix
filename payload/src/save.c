#include "global.h"
#include "main.h"
#include "save.h"
#include "rtc.h"

struct SaveBlockChunk
{
    u8 * data;
    u16 size;
};

u8 WriteSaveBlockChunks(u16, const struct SaveBlockChunk *);
u8 WriteSingleChunk(u16, const struct SaveBlockChunk *);
u8 TryWriteSector(u8, u8 *);
static u8 HandleReplaceSector(u16, const struct SaveBlockChunk *);
u8 TryReadAllSaveSectorsCurrentSlot(u16, const struct SaveBlockChunk *);
u8 ReadAllSaveSectorsCurrentSlot(u16, const struct SaveBlockChunk *);
u8 GetSaveValidStatus(const struct SaveBlockChunk *);
u32 DoReadFlashWholeSection(u8, struct SaveSector *);
u16 CalculateChecksum(const void *, u16);

u16 gLastWrittenSector;
u32 gPrevSaveCounter;
u16 gLastKnownGoodSector;
u32 gDamagedSaveSectors;
u32 gSaveCounter;
struct SaveSector * gReadWriteSector;
u16 gCurSaveChunk;
bool32 gFlashIdentIsValid;

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

bool32 flash_maincb_ident_is_valid(void)
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

void Call_ReadFlash(u16 sectorNum, ptrdiff_t offset, void * dest, size_t size)
{
    ReadFlash(sectorNum, offset, dest, size);
}

u8 Call_WriteSaveBlockChunks(u16 a0, const struct SaveBlockChunk * a1)
{
    return WriteSaveBlockChunks(a0, a1);
}

u8 Call_TryReadAllSaveSectorsCurrentSlot(u16 a0, const struct SaveBlockChunk * a1)
{
    return TryReadAllSaveSectorsCurrentSlot(a0, a1);
}

u32 * GetDamagedSaveSectorsPtr(void)
{
    return &gDamagedSaveSectors;
}

s32 flash_write_save_block_chunks(u8 a0)
{
    u8 i;

    switch (a0)
    {
        case 0:
        default:
            Call_WriteSaveBlockChunks(0xFFFF, sSaveBlockChunks);
            break;
        case 1:
            for (i = 0; i < 5; i++)
            {
                Call_WriteSaveBlockChunks(i, sSaveBlockChunks);
            }
            break;
        case 2:
            Call_WriteSaveBlockChunks(0, sSaveBlockChunks);
            break;
    }

    return 0;
}

u8 flash_write_save_block_chunks_check_damage(u8 a0)
{
    flash_write_save_block_chunks(a0);
    if (*GetDamagedSaveSectorsPtr() == 0)
        return 1;
    return 0xFF;
}

u8 flash_maincb_read_save(u32 unused)
{
    return Call_TryReadAllSaveSectorsCurrentSlot(0xFFFF, sSaveBlockChunks);
}

void msg_load_gfx(void)
{
    REG_DISPCNT = 0;
    REG_BG0HOFS = 0;
    REG_BG0VOFS = 0;
    REG_BLDCNT = 0;
    LZ77UnCompVram(gInfoMessagesGfx, (void *)BG_VRAM);
    LZ77UnCompVram(gInfoMessagesTilemap, (void *)BG_SCREEN_ADDR(28));
    CpuCopy16(gInfoMessagesPal, (void *)BG_PLTT, 0x200);
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

void Save_EraseAllData(void)
{
    u16 i;
    for (i = 0; i < 32; i++)
        EraseFlashSector(i);
}

void Save_ResetSaveCounters(void)
{
    gSaveCounter = 0;
    gLastWrittenSector = 0;
    gDamagedSaveSectors = 0;
}

bool32 SetSectorDamagedStatus(u8 op, u8 sectorNum)
{
    bool32 retVal = FALSE;

    switch (op)
    {
        case SECTOR_DAMAGED:
            gDamagedSaveSectors |= (1 << sectorNum);
            break;
        case SECTOR_OK:
            gDamagedSaveSectors &= ~(1 << sectorNum);
            break;
        case SECTOR_CHECK: // unused
            if (gDamagedSaveSectors & (1 << sectorNum))
                retVal = TRUE;
            break;
    }

    return retVal;
}

u8 WriteSaveBlockChunks(u16 chunkId, const struct SaveBlockChunk *chunks)
{
    u32 retVal;
    u16 i;

    gReadWriteSector = eSaveSection;

    if (chunkId != 0xFFFF)  // write single chunk
    {
        retVal = WriteSingleChunk(chunkId, chunks);
    }
    else  // write all chunks
    {
        gLastKnownGoodSector = gLastWrittenSector;
        gPrevSaveCounter = gSaveCounter;
        gLastWrittenSector++;
        gLastWrittenSector %= NUM_SECTORS_PER_SLOT;
        gSaveCounter++;
        retVal = SAVE_STATUS_OK;

        for (i = 0; i < NUM_SECTORS_PER_SLOT; i++)
            WriteSingleChunk(i, chunks);

        // Check for any bad sectors
        if (gDamagedSaveSectors != 0) // skip the damaged sector.
        {
            retVal = SAVE_STATUS_ERROR;
            gLastWrittenSector = gLastKnownGoodSector;
            gSaveCounter = gPrevSaveCounter;
        }
    }

    return retVal;
}

u8 WriteSingleChunk(u16 chunkId, const struct SaveBlockChunk * chunks)
{
    u16 i;
    u16 sectorNum;
    u8 *chunkData;
    u16 chunkSize;

    // select sector number
    sectorNum = chunkId + gLastWrittenSector;
    sectorNum %= NUM_SECTORS_PER_SLOT;
    // select save slot
    sectorNum += NUM_SECTORS_PER_SLOT * (gSaveCounter % 2);

    chunkData = chunks[chunkId].data;
    chunkSize = chunks[chunkId].size;

    // clear save section.
    for (i = 0; i < sizeof(struct SaveSector); i++)
        ((u8 *)gReadWriteSector)[i] = 0;

    gReadWriteSector->id = chunkId;
    gReadWriteSector->security = SECTOR_SECURITY_NUM;
    gReadWriteSector->counter = gSaveCounter;
    for (i = 0; i < chunkSize; i++)
        gReadWriteSector->data[i] = chunkData[i];
    gReadWriteSector->checksum = CalculateChecksum(chunkData, chunkSize);

    return TryWriteSector(sectorNum, gReadWriteSector->data);
}

u8 HandleWriteSectorNBytes(u8 sectorNum, u8 *data, u16 size)
{
    u16 i;
    struct SaveSector *section = eSaveSection;

    for (i = 0; i < sizeof(struct SaveSector); i++)
        ((char *)section)[i] = 0;

    section->security = SECTOR_SECURITY_NUM;
    for (i = 0; i < size; i++)
        section->data[i] = data[i];
    section->id = CalculateChecksum(data, size); // though this appears to be incorrect, it might be some sector checksum instead of a whole save checksum and only appears to be relevent to HOF data, if used.

    return TryWriteSector(sectorNum, section->data);
}

u8 TryWriteSector(u8 sectorNum, u8 *data)
{
    if (ProgramFlashSectorAndVerify(sectorNum, data) != 0) // is damaged?
    {
        SetSectorDamagedStatus(SECTOR_DAMAGED, sectorNum); // set damaged sector bits.
        return SAVE_STATUS_ERROR;
    }
    else
    {
        SetSectorDamagedStatus(SECTOR_OK, sectorNum); // unset damaged sector bits. it's safe now.
        return SAVE_STATUS_OK;
    }
}

u32 RestoreSaveBackupVarsAndIncrement(const struct SaveBlockChunk *chunk) // chunk is unused
{
    gReadWriteSector = eSaveSection;
    gLastKnownGoodSector = gLastWrittenSector;
    gPrevSaveCounter = gSaveCounter;
    gLastWrittenSector++;
    gLastWrittenSector %= NUM_SECTORS_PER_SLOT;
    gSaveCounter++;
    gCurSaveChunk = 0;
    gDamagedSaveSectors = 0;
    return 0;
}

u32 RestoreSaveBackupVars(const struct SaveBlockChunk *chunk)
{
    gReadWriteSector = eSaveSection;
    gLastKnownGoodSector = gLastWrittenSector;
    gPrevSaveCounter = gSaveCounter;
    gCurSaveChunk = 0;
    gDamagedSaveSectors = 0;
    return 0;
}

u8 WriteSingleChunkAndIncrement(u16 a1, const struct SaveBlockChunk * chunk)
{
    u8 retVal;

    if (gCurSaveChunk < a1 - 1)
    {
        retVal = SAVE_STATUS_OK;
        WriteSingleChunk(gCurSaveChunk, chunk);
        gCurSaveChunk++;
        if (gDamagedSaveSectors)
        {
            retVal = SAVE_STATUS_ERROR;
            gLastWrittenSector = gLastKnownGoodSector;
            gSaveCounter = gPrevSaveCounter;
        }
    }
    else
    {
        retVal = SAVE_STATUS_ERROR;
    }

    return retVal;
}

u8 ErasePreviousChunk(u16 a1, const struct SaveBlockChunk *chunk)
{
    u8 retVal = SAVE_STATUS_OK;

    HandleReplaceSector(a1 - 1, chunk);

    if (gDamagedSaveSectors)
    {
        retVal = SAVE_STATUS_ERROR;
        gLastWrittenSector = gLastKnownGoodSector;
        gSaveCounter = gPrevSaveCounter;
    }
    return retVal;
}

static u8 HandleReplaceSector(u16 sectorId, const struct SaveBlockChunk *locations)
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
    data = locations[sectorId].data;
    size = locations[sectorId].size;

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
        SetSectorDamagedStatus(SECTOR_DAMAGED, sector);
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
            SetSectorDamagedStatus(SECTOR_DAMAGED, sector);
            return SAVE_STATUS_ERROR;
        }
        else
        {
            // Succeeded
            SetSectorDamagedStatus(SECTOR_OK, sector);
            return SAVE_STATUS_OK;
        }
    }
}

static u8 CopySectorSecurityByte(u16 sectorId, const struct SaveBlockChunk *locations)
{
    // Adjust sector id for current save slot
    u16 sector = sectorId + gLastWrittenSector - 1;
    sector %= NUM_SECTORS_PER_SLOT;
    sector += NUM_SECTORS_PER_SLOT * (gSaveCounter % NUM_SAVE_SLOTS);

    // Copy just the first byte of the security field from the read/write buffer
    if (ProgramFlashByte(sector, SECTOR_SECURITY_OFFSET, ((u8 *)gReadWriteSector)[SECTOR_SECURITY_OFFSET]))
    {
        // Sector is damaged, so enable the bit in gDamagedSaveSectors and restore the last written sector and save counter.
        SetSectorDamagedStatus(SECTOR_DAMAGED, sector);
        gLastWrittenSector = gLastKnownGoodSector;
        gSaveCounter = gPrevSaveCounter;
        return SAVE_STATUS_ERROR;
    }
    else
    {
        SetSectorDamagedStatus(SECTOR_OK, sector);
        return SAVE_STATUS_OK;
    }
}

static u8 WriteSectorSecurityByte(u16 sectorId, const struct SaveBlockChunk *locations)
{
    // Adjust sector id for current save slot
    u16 sector = sectorId + gLastWrittenSector - 1;
    sector %= NUM_SECTORS_PER_SLOT;
    sector += NUM_SECTORS_PER_SLOT * (gSaveCounter % 2);

    // Write just the first byte of the security field, which was skipped by HandleReplaceSector
    if (ProgramFlashByte(sector, SECTOR_SECURITY_OFFSET, SECTOR_SECURITY_NUM & 0xFF))
    {
        // Sector is damaged, so enable the bit in gDamagedSaveSectors and restore the last written sector and save counter.
        SetSectorDamagedStatus(SECTOR_DAMAGED, sector);
        gLastWrittenSector = gLastKnownGoodSector;
        gSaveCounter = gPrevSaveCounter;
        return SAVE_STATUS_ERROR;
    }
    else
    {
        // Succeeded
        SetSectorDamagedStatus(SECTOR_OK, sector);
        return SAVE_STATUS_OK;
    }
}

u8 TryReadAllSaveSectorsCurrentSlot(u16 a1, const struct SaveBlockChunk *chunk)
{
    u8 retVal;
    gReadWriteSector = eSaveSection;
    if (a1 != 0xFFFF)
    {
        retVal = SAVE_STATUS_ERROR;
    }
    else
    {
        retVal = GetSaveValidStatus(chunk);
        ReadAllSaveSectorsCurrentSlot(0xFFFF, chunk);
    }

    return retVal;
}

u8 ReadAllSaveSectorsCurrentSlot(u16 a1, const struct SaveBlockChunk *chunks)
{
    u16 i;
    u16 checksum;
    u16 sector = NUM_SECTORS_PER_SLOT * (gSaveCounter % 2);
    u16 id;

    for (i = 0; i < NUM_SECTORS_PER_SLOT; i++)
    {
        DoReadFlashWholeSection(i + sector, gReadWriteSector);
        id = gReadWriteSector->id;
        if (id == 0)
            gLastWrittenSector = i;
        checksum = CalculateChecksum(gReadWriteSector->data, chunks[id].size);
        if (gReadWriteSector->security == SECTOR_SECURITY_NUM
            && gReadWriteSector->checksum == checksum)
        {
            u16 j;
            for (j = 0; j < chunks[id].size; j++)
                chunks[id].data[j] = gReadWriteSector->data[j];
        }
    }

    return 1;
}

u8 GetSaveValidStatus(const struct SaveBlockChunk *chunks)
{
    u16 sector;
    bool8 signatureValid;
    u16 checksum;
    u32 slot1saveCounter = 0;
    u32 slot2saveCounter = 0;
    u8 slot1Status;
    u8 slot2Status;
    u32 validSectors;
    const u32 ALL_SECTORS = (1 << NUM_SECTORS_PER_SLOT) - 1;  // bitmask of all saveblock sectors

    // check save slot 1.
    validSectors = 0;
    signatureValid = FALSE;
    for (sector = 0; sector < NUM_SECTORS_PER_SLOT; sector++)
    {
        DoReadFlashWholeSection(sector, gReadWriteSector);
        if (gReadWriteSector->security == SECTOR_SECURITY_NUM)
        {
            signatureValid = TRUE;
            checksum = CalculateChecksum(gReadWriteSector->data, chunks[gReadWriteSector->id].size);
            if (gReadWriteSector->checksum == checksum)
            {
                slot1saveCounter = gReadWriteSector->counter;
                validSectors |= 1 << gReadWriteSector->id;
            }
        }
    }

    if (signatureValid)
    {
        if (validSectors == ALL_SECTORS)
            slot1Status = SAVE_STATUS_OK;
        else
            slot1Status = SAVE_STATUS_ERROR;
    }
    else
    {
        slot1Status = SAVE_STATUS_EMPTY;
    }

    // check save slot 2.
    validSectors = 0;
    signatureValid = FALSE;
    for (sector = 0; sector < NUM_SECTORS_PER_SLOT; sector++)
    {
        DoReadFlashWholeSection(NUM_SECTORS_PER_SLOT + sector, gReadWriteSector);
        if (gReadWriteSector->security == SECTOR_SECURITY_NUM)
        {
            signatureValid = TRUE;
            checksum = CalculateChecksum(gReadWriteSector->data, chunks[gReadWriteSector->id].size);
            if (gReadWriteSector->checksum == checksum)
            {
                slot2saveCounter = gReadWriteSector->counter;
                validSectors |= 1 << gReadWriteSector->id;
            }
        }
    }

    if (signatureValid)
    {
        if (validSectors == ALL_SECTORS)
            slot2Status = SAVE_STATUS_OK;
        else
            slot2Status = SAVE_STATUS_ERROR;
    }
    else
    {
        slot2Status = SAVE_STATUS_EMPTY;
    }

    if (slot1Status == SAVE_STATUS_OK && slot2Status == SAVE_STATUS_OK)
    {
        // Choose counter of the most recent save file
        if ((slot1saveCounter == -1 && slot2saveCounter == 0) || (slot1saveCounter == 0 && slot2saveCounter == -1))
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

    if (slot1Status == SAVE_STATUS_OK)
    {
        gSaveCounter = slot1saveCounter;
        if (slot2Status == SAVE_STATUS_ERROR)
            return SAVE_STATUS_ERROR;
        else
            return SAVE_STATUS_OK;
    }

    if (slot2Status == SAVE_STATUS_OK)
    {
        gSaveCounter = slot2saveCounter;
        if (slot1Status == SAVE_STATUS_ERROR)
            return SAVE_STATUS_ERROR;
        else
            return SAVE_STATUS_OK;
    }

    if (slot1Status == SAVE_STATUS_EMPTY && slot2Status == SAVE_STATUS_EMPTY)
    {
        gSaveCounter = 0;
        gLastWrittenSector = 0;
        return SAVE_STATUS_EMPTY;
    }

    gSaveCounter = 0;
    gLastWrittenSector = 0;
    return 2;
}

u8 ReadSomeUnknownSectorAndVerify(u8 sector, u8 *data, u16 size)
{
    u16 i;
    struct SaveSector *section = eSaveSection;

    DoReadFlashWholeSection(sector, section);
    if (section->security == SECTOR_SECURITY_NUM)
    {
        u16 checksum = CalculateChecksum(section->data, size);
        if (section->id == checksum)
        {
            for (i = 0; i < size; i++)
                data[i] = section->data[i];
            return SAVE_STATUS_OK;
        }
        else
        {
            return 2;
        }
    }
    else
    {
        return SAVE_STATUS_EMPTY;
    }
}

u32 DoReadFlashWholeSection(u8 sector, struct SaveSector *section)
{
    ReadFlash(sector, 0, section->data, sizeof(struct SaveSector));
    return 1;
}

u16 CalculateChecksum(const void *data, u16 size)
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

void nullsub_0201182C()
{
}

void nullsub_02011830()
{
}

void nullsub_02011834()
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
    if (flash_write_save_block_chunks_check_damage(0) != TRUE)
        return FALSE;
    return TRUE;
}

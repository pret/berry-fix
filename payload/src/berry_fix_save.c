#include "global.h"
#include "main.h"
#include "berry_fix_save.h"

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
        BerryFix_WriteSaveSectorOrSlot(FULL_SAVE_SLOT, gSaveBlockChunks);
        break;
    case SAVE_SAVEBLOCKS:
        for (i = SECTOR_ID_SAVEBLOCK2; i <= SECTOR_ID_SAVEBLOCK1_END; i++)
            BerryFix_WriteSaveSectorOrSlot(i, gSaveBlockChunks);
        break;
    case SAVE_SAVEBLOCK2:
        BerryFix_WriteSaveSectorOrSlot(SECTOR_ID_SAVEBLOCK2, gSaveBlockChunks);
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
    return BerryFix_TryLoadSaveSlot(FULL_SAVE_SLOT, gSaveBlockChunks);
}

#ifndef GUARD_BERRY_FIX_SAVE_H
#define GUARD_BERRY_FIX_SAVE_H

#include "save.h"

enum
{
    SAVE_NORMAL,     // Save full save slot
    SAVE_SAVEBLOCKS, // Save just SaveBlock1 and SaveBlock2
    SAVE_SAVEBLOCK2, // Save just SaveBlock2
};

bool32 BerryFix_IdentifyFlash(void);
u8 BerryFix_TrySave(u8 mode);
bool8 BerryFix_LoadSave(u32);

#endif  //GUARD_BERRY_FIX_SAVE_H

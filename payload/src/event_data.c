#include "global.h"
#include "rtc.h"
#include "berry_fix_save.h"
#include "event_data.h"

// Unused
static void Dummy1()
{
}

// Unused
static void Dummy2()
{
}

// Unused
static void Dummy3()
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

bool32 BerryFix_IsPacifidlogTMCorrect(void)
{
    u8 year;
    u16 * var = GetVarPointer(VAR_PACIFIDLOG_TM_RECEIVED_DAY);
    BerryFix_CalcTimeDifference(&year);
    if (*var <= gRtcUTCTime.days)
        return TRUE;
    else
        return FALSE;
}

bool32 BerryFix_ResetPacifidlogTM(void)
{
    u8 year;
    if (BerryFix_IsPacifidlogTMCorrect() == TRUE)
        return TRUE;
    BerryFix_CalcTimeDifference(&year);
    if (gRtcUTCTime.days < 0)
        return FALSE;
    *GetVarPointer(VAR_PACIFIDLOG_TM_RECEIVED_DAY) = 1;
    if (BerryFix_TrySave(SAVE_NORMAL) != SAVE_STATUS_OK)
        return FALSE;
    return TRUE;
}

#include "global.h"
#include "siirtc.h"
#include "main.h"
#include "rtc.h"

struct Time gTimeSinceBerryUpdate;
struct Time gRtcUTCTime;

static u16 sErrorStatus;
static struct SiiRtcInfo sRtcInfoBuffer;
static u8 sProbeResult;
static u16 sSavedIme;
static struct SiiRtcInfo sRtcInfoWork;

static const struct SiiRtcInfo sDefaultRTC = {0, MONTH_JAN, 1}; // 2000 Jan 1

static const s32 sDaysPerMonth[] = {
    [MONTH_JAN - 1] = 31,
    [MONTH_FEB - 1] = 28,
    [MONTH_MAR - 1] = 31,
    [MONTH_APR - 1] = 30,
    [MONTH_MAY - 1] = 31,
    [MONTH_JUN - 1] = 30,
    [MONTH_JUL - 1] = 31,
    [MONTH_AUG - 1] = 31,
    [MONTH_SEP - 1] = 30,
    [MONTH_OCT - 1] = 31,
    [MONTH_NOV - 1] = 30,
    [MONTH_DEC - 1] = 31
};

static void RtcGetRawInfo(struct SiiRtcInfo *);
static u16 RtcCheckInfo(struct SiiRtcInfo *);

static void RtcDisableInterrupts(void)
{
    sSavedIme = REG_IME;
    REG_IME = 0;
}

static void RtcRestoreInterrupts(void)
{
    REG_IME = sSavedIme;
}

static s32 ConvertBcdToBinary(u8 bcd)
{
    if (bcd >= 0xa0 || (bcd & 0xF) >= 10)
        return 0xFF;
    return ((bcd >> 4) & 0xF) * 10 + (bcd & 0xF);
}

static bool8 IsLeapYear(u8 year)
{
    if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)
        return TRUE;
    return FALSE;
}

static u16 ConvertDateToDayCount(u8 year, u8 month, u8 day)
{
    s32 i;
    u16 dayCount = 0;

    for (i = year - 1; i > 0; i--)
    {
        dayCount += 365;

        if (IsLeapYear(i) == TRUE)
            dayCount++;
    }

    for (i = 0; i < month - 1; i++)
        dayCount += sDaysPerMonth[i];

    if (month > MONTH_FEB && IsLeapYear(year) == TRUE)
        dayCount++;

    dayCount += day;

    return dayCount;
}

u16 RtcGetDayCount(struct SiiRtcInfo *rtc)
{
    u8 year = ConvertBcdToBinary(rtc->year);
    u8 month = ConvertBcdToBinary(rtc->month);
    u8 day = ConvertBcdToBinary(rtc->day);
    return ConvertDateToDayCount(year, month, day);}

static void RtcInit(void)
{
    sErrorStatus = 0;

    RtcDisableInterrupts();
    SiiRtcUnprotect();
    sProbeResult = SiiRtcProbe();
    RtcRestoreInterrupts();

    if ((sProbeResult & 0xF) != 1)
    {
        sErrorStatus = RTC_INIT_ERROR;
        return;
    }

    if (sProbeResult & 0xF0)
        sErrorStatus = RTC_INIT_WARNING;
    else
        sErrorStatus = 0;

    RtcGetRawInfo(&sRtcInfoBuffer);
    sErrorStatus = RtcCheckInfo(&sRtcInfoBuffer);
}

static u16 RtcGetErrorStatus(void)
{
    return sErrorStatus;
}

// Unused
static void RtcGetInfo(struct SiiRtcInfo * rtc)
{
    if (sErrorStatus & RTC_ERR_FLAG_MASK)
        *rtc = sDefaultRTC;
    else
        RtcGetRawInfo(rtc);
}

static void RtcGetDateTime(struct SiiRtcInfo * rtc)
{
    RtcDisableInterrupts();
    SiiRtcGetDateTime(rtc);
    RtcRestoreInterrupts();
}

static void RtcGetStatus(struct SiiRtcInfo * rtc)
{
    RtcDisableInterrupts();
    SiiRtcGetStatus(rtc);
    RtcRestoreInterrupts();
}

static void RtcGetRawInfo(struct SiiRtcInfo * rtc)
{
    RtcGetStatus(rtc);
    RtcGetDateTime(rtc);
}

static u16 RtcCheckInfo(struct SiiRtcInfo * rtc)
{
    u16 errorFlags = 0;
    s32 year;
    s32 month;
    s32 value;

    if (rtc->status & SIIRTCINFO_POWER)
        errorFlags |= RTC_ERR_POWER_FAILURE;

    if (!(rtc->status & SIIRTCINFO_24HOUR))
        errorFlags |= RTC_ERR_12HOUR_CLOCK;

    year = ConvertBcdToBinary(rtc->year);
    if (year == 0xFF)
        errorFlags |= RTC_ERR_INVALID_YEAR;

    month = ConvertBcdToBinary(rtc->month);
    if (month == 0xFF || month == 0 || month > 12)
        errorFlags |= RTC_ERR_INVALID_MONTH;

    value = ConvertBcdToBinary(rtc->day);
    if (value == 0xFF)
        errorFlags |= RTC_ERR_INVALID_DAY;

    if (month == MONTH_FEB)
    {
        if (value > IsLeapYear(year) + sDaysPerMonth[1])
            errorFlags |= RTC_ERR_INVALID_DAY;
    }
    else
    {
        if (value > sDaysPerMonth[month - 1])
            errorFlags |= RTC_ERR_INVALID_DAY;
    }

    value = ConvertBcdToBinary(rtc->hour);
    if (value > 24)
        errorFlags |= RTC_ERR_INVALID_HOUR;

    value = ConvertBcdToBinary(rtc->minute);
    if (value > 60)
        errorFlags |= RTC_ERR_INVALID_MINUTE;

    value = ConvertBcdToBinary(rtc->second);
    if (value > 60)
        errorFlags |= RTC_ERR_INVALID_SECOND;

    return errorFlags;
}

// Unused
static void RtcReset(void)
{
    RtcDisableInterrupts();
    SiiRtcReset();
    RtcRestoreInterrupts();
}

static void RtcCalcTimeDifference(struct SiiRtcInfo * rtc, struct Time * result, struct Time * t)
{
    u16 days = RtcGetDayCount(rtc);
    result->seconds = ConvertBcdToBinary(rtc->second) - t->seconds;
    result->minutes = ConvertBcdToBinary(rtc->minute) - t->minutes;
    result->hours = ConvertBcdToBinary(rtc->hour) - t->hours;
    result->days = days - t->days;

    if (result->seconds < 0)
    {
        result->seconds += 60;
        result->minutes--;
    }

    if (result->minutes < 0)
    {
        result->minutes += 60;
        result->hours--;
    }

    if (result->hours < 0)
    {
        result->hours += 24;
        result->days--;
    }
}

static void CalcTimeDifference(struct Time * result, struct Time * t1, struct Time * t2)
{
    result->seconds = t2->seconds - t1->seconds;
    result->minutes = t2->minutes - t1->minutes;
    result->hours = t2->hours - t1->hours;
    result->days = t2->days - t1->days;

    if (result->seconds < 0)
    {
        result->seconds += 60;
        result->minutes--;
    }

    if (result->minutes < 0)
    {
        result->minutes += 60;
        result->hours--;
    }

    if (result->hours < 0)
    {
        result->hours += 24;
        result->days--;
    }
}

// New code for Berry Fix Program starts here

bool32 BerryFix_TryInitRtc(void)
{
    RtcInit();
    if (RtcGetErrorStatus() & RTC_ERR_FLAG_MASK)
        return FALSE;
    return TRUE;
}

static void RtcSetDateTime(struct SiiRtcInfo * rtc)
{
    vu16 imeBak = REG_IME;
    REG_IME = 0;
    SiiRtcSetDateTime(rtc);
    REG_IME = imeBak;
}

// The below are equivalent to &gSaveBlock2.localTimeOffset and &gSaveBlock2.lastBerryTreeUpdate.
// Replacing them both below doesn't match
#define SaveBlock2Addr      (EWRAM_START + 0x28000)
#define LocalTimeOffset     ((struct Time *)(SaveBlock2Addr + offsetof(struct SaveBlock2, localTimeOffset)))
#define LastBerryTreeUpdate ((struct Time *)(SaveBlock2Addr + offsetof(struct SaveBlock2, lastBerryTreeUpdate)))

bool32 BerryFix_CalcTimeDifference(u8 * year)
{
    RtcGetRawInfo(&sRtcInfoWork);
    *year = ConvertBcdToBinary(sRtcInfoWork.year);
    RtcCalcTimeDifference(&sRtcInfoWork, &gRtcUTCTime, LocalTimeOffset);
    CalcTimeDifference(&gTimeSinceBerryUpdate, LastBerryTreeUpdate, &gRtcUTCTime);
    if (gTimeSinceBerryUpdate.days * 24 * 60 + gTimeSinceBerryUpdate.hours * 60 + gTimeSinceBerryUpdate.minutes >= 0)
        return TRUE;
    return FALSE;
}

static u32 ConvertBinaryToBcd(u8 binary)
{
    u32 bcd;
    if (binary > 99)
        return 0xFF;
    bcd = Div(binary, 10) << 4;
    bcd |= Mod(binary, 10);
    return bcd;
}

static void RtcIncrement(u8 * val)
{
    *val = ConvertBinaryToBcd(ConvertBcdToBinary(*val) + 1);
}

static void RtcIncrementMonth(struct SiiRtcInfo * rtc)
{
    RtcIncrement(&rtc->month);
    if (ConvertBcdToBinary(rtc->month) > 12)
    {
        RtcIncrement(&rtc->year);
        rtc->month = MONTH_JAN;
    }
}

static void RtcIncrementDay(struct SiiRtcInfo * rtc)
{
    RtcIncrement(&rtc->day);
    if (ConvertBcdToBinary(rtc->day) > sDaysPerMonth[ConvertBcdToBinary(rtc->month) - 1])
    {
        if (!IsLeapYear(ConvertBcdToBinary(rtc->year)) || ConvertBcdToBinary(rtc->month) != MONTH_FEB || ConvertBcdToBinary(rtc->day) != 29)
        {
            rtc->day = 1;
            RtcIncrementMonth(rtc);
        }
    }
}

// When fixing the RTC, consider if the leap day on Feb 29, 2000 was reached
static bool32 RtcNeedsLeapDayIncrement(struct SiiRtcInfo * rtc)
{
    if (ConvertBcdToBinary(rtc->year) == 0)
    {
        if (ConvertBcdToBinary(rtc->month) == MONTH_JAN)
            return FALSE;
        if (ConvertBcdToBinary(rtc->month) > MONTH_FEB)
            return TRUE;
        if (ConvertBcdToBinary(rtc->day) == 29)
            return TRUE;
        return FALSE;
    }
    if (ConvertBcdToBinary(rtc->year) == 1)
        return TRUE;

    // After 2001 (shouldn't occur)
    return FALSE;
}

void BerryFix_TryFixDate(void)
{
    RtcGetRawInfo(&sRtcInfoWork);

    // If the year is anything but 2000 or 2001 then the Berry Glitch has already passed
    if (ConvertBcdToBinary(sRtcInfoWork.year) == 0 || ConvertBcdToBinary(sRtcInfoWork.year) == 1)
    {
        if (ConvertBcdToBinary(sRtcInfoWork.year) == 1)
        {
            // Year is 2001, the Berry Glitch is occurring
            // Set date forward to January 2, 2002
            sRtcInfoWork.year = 2;
            sRtcInfoWork.month = MONTH_JAN;
            sRtcInfoWork.day = 2;
            RtcSetDateTime(&sRtcInfoWork);
        }
        else
        {
            // Year is 2000, the Berry Glitch hasn't begun yet
            // Set the date forward 365/366 days to avoid the glitch
            if (RtcNeedsLeapDayIncrement(&sRtcInfoWork) == TRUE)
            {
                RtcIncrementDay(&sRtcInfoWork);
                RtcIncrement(&sRtcInfoWork.year);
            }
            else
            {
                RtcIncrement(&sRtcInfoWork.year);
            }
            RtcSetDateTime(&sRtcInfoWork);
        }
    }
}

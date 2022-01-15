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
    if (sErrorStatus & 0xFF0)
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

bool32 rtc_maincb_is_rtc_working(void)
{
    RtcInit();
    if (RtcGetErrorStatus() & 0xFF0)
        return FALSE;
    return TRUE;
}

void rtc_set_datetime(struct SiiRtcInfo * rtc)
{
    vu16 imeBak = REG_IME;
    REG_IME = 0;
    SiiRtcSetDateTime(rtc);
    REG_IME = imeBak;
}

bool32 rtc_maincb_is_time_since_last_berry_update_positive(u8 * year)
{
    RtcGetRawInfo(&sRtcInfoWork);
    *year = ConvertBcdToBinary(sRtcInfoWork.year);
    RtcCalcTimeDifference(&sRtcInfoWork, &gRtcUTCTime, LocalTimeOffset);
    CalcTimeDifference(&gTimeSinceBerryUpdate, LastBerryTreeUpdate, &gRtcUTCTime);
    if (gTimeSinceBerryUpdate.days * 1440 + gTimeSinceBerryUpdate.hours * 60 + gTimeSinceBerryUpdate.minutes >= 0)
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

void sii_rtc_inc(u8 * a0)
{
    *a0 = ConvertBinaryToBcd(ConvertBcdToBinary(*a0) + 1);
}

void sii_rtc_inc_month(struct SiiRtcInfo * a0)
{
    sii_rtc_inc(&a0->month);
    if (ConvertBcdToBinary(a0->month) > 12)
    {
        sii_rtc_inc(&a0->year);
        a0->month = MONTH_JAN;
    }
}

void sii_rtc_inc_day(struct SiiRtcInfo * a0)
{
    sii_rtc_inc(&a0->day);
    if (ConvertBcdToBinary(a0->day) > sDaysPerMonth[ConvertBcdToBinary(a0->month) - 1])
    {
        if (!IsLeapYear(ConvertBcdToBinary(a0->year)) || ConvertBcdToBinary(a0->month) != MONTH_FEB || ConvertBcdToBinary(a0->day) != 29)
        {
            a0->day = 1;
            sii_rtc_inc_month(a0);
        }
    }
}

bool32 rtc_is_past_feb_28_2000(struct SiiRtcInfo * a0)
{
    if (ConvertBcdToBinary(a0->year) == 0)
    {
        if (ConvertBcdToBinary(a0->month) == MONTH_JAN)
            return FALSE;
        if (ConvertBcdToBinary(a0->month) > MONTH_FEB)
            return TRUE;
        if (ConvertBcdToBinary(a0->day) == 29)
            return TRUE;
        return FALSE;
    }
    if (ConvertBcdToBinary(a0->year) == 1)
        return TRUE;
    return FALSE;
}

void rtc_maincb_fix_date(void)
{
    RtcGetRawInfo(&sRtcInfoWork);
    if (ConvertBcdToBinary(sRtcInfoWork.year) == 0 || ConvertBcdToBinary(sRtcInfoWork.year) == 1)
    {
        if (ConvertBcdToBinary(sRtcInfoWork.year) == 1)
        {
            sRtcInfoWork.year = 2;
            sRtcInfoWork.month = MONTH_JAN;
            sRtcInfoWork.day = 2;
            rtc_set_datetime(&sRtcInfoWork);
        }
        else
        {
            if (rtc_is_past_feb_28_2000(&sRtcInfoWork) == TRUE)
            {
                sii_rtc_inc_day(&sRtcInfoWork);
                sii_rtc_inc(&sRtcInfoWork.year);
            }
            else
            {
                sii_rtc_inc(&sRtcInfoWork.year);
            }
            rtc_set_datetime(&sRtcInfoWork);
        }
    }
}
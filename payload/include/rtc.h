#ifndef GUARD_RTC_H
#define GUARD_RTC_H

#define RTC_INIT_ERROR         0x0001
#define RTC_INIT_WARNING       0x0002

#define RTC_ERR_12HOUR_CLOCK   0x0010
#define RTC_ERR_POWER_FAILURE  0x0020
#define RTC_ERR_INVALID_YEAR   0x0040
#define RTC_ERR_INVALID_MONTH  0x0080
#define RTC_ERR_INVALID_DAY    0x0100
#define RTC_ERR_INVALID_HOUR   0x0200
#define RTC_ERR_INVALID_MINUTE 0x0400
#define RTC_ERR_INVALID_SECOND 0x0800

#define RTC_ERR_FLAG_MASK      0x0FF0

extern struct Time gTimeSinceBerryUpdate;
extern struct Time gRtcUTCTime;

bool32 BerryFix_TryInitRtc(void);
void BerryFix_TryFixDate(void);
bool32 BerryFix_CalcTimeDifference(u8 *);

#endif //GUARD_RTC_H

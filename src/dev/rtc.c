#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <sys/errno.h>

#include <cosec/log.h>

#include "dev/cmos.h"

/*
 *  Read time from CMOS
 */

static uint8_t from_bcd(uint8_t bcd) {
    return 10 * (bcd / 0x10) + (bcd % 0x10);
}

static inline void convert_from_cmos_fmt(ymd_hms *time) {
    uint8_t cmos_b = read_cmos(CMOS_REG_STATUS_B);
    logmsgdf("cmos b = %x\n", (uint)cmos_b);

    if (!(cmos_b & CMOS_B_FMT_NOT_BCD)) {
        time->tm_sec = from_bcd(time->tm_sec);
        time->tm_min = from_bcd(time->tm_min);
        time->tm_hour  = from_bcd(time->tm_hour);
        time->tm_mday = from_bcd(time->tm_mday);
        time->tm_mon = from_bcd(time->tm_mon);
        time->tm_year = from_bcd(time->tm_year);
    }

    /* time to 2070 is more than enough for this system **/
    time->tm_year += (time->tm_year >= 70 ? 1900 : 2000);
}

err_t time_ymd_from_rtc(ymd_hms *ymd) {
    struct tm time;

    do {
        logmsgd("RTC read: trying...");
        while (cmos_is_in_update());
        time.tm_sec = read_cmos(CMOS_REG_SECS);
        time.tm_min = read_cmos(CMOS_REG_MINS);
        time.tm_hour = read_cmos(CMOS_REG_HOURS);
        time.tm_mday = read_cmos(CMOS_REG_DAY_OF_MONTH);
        time.tm_mon = read_cmos(CMOS_REG_MONTH);
        time.tm_year = read_cmos(CMOS_REG_YEAR);

        while (cmos_is_in_update());
        ymd->tm_sec = read_cmos(CMOS_REG_SECS);
        ymd->tm_min = read_cmos(CMOS_REG_MINS);
        ymd->tm_hour = read_cmos(CMOS_REG_HOURS);
        ymd->tm_mday = read_cmos(CMOS_REG_DAY_OF_MONTH);
        ymd->tm_mon = read_cmos(CMOS_REG_MONTH);
        ymd->tm_year = read_cmos(CMOS_REG_YEAR);
    } while(!memcmp(&time, ymd, sizeof(ymd_hms)));

    convert_from_cmos_fmt(ymd);

    return NOERR;
}

time_t unix_time(void) {
    ymd_hms t;
    time_ymd_from_rtc(&t);

    return mktime(&t);
}

time_t time(time_t *t) {
    time_t _t = unix_time();
    if (t) *t = _t;
    return _t;
}

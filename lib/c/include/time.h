#ifndef __TIME_H__
#define __TIME_H__

#include <stdint.h>

#define CLOCKS_PER_SEC  1000000

typedef unsigned long       time_t;     /* seconds since the epoch */
typedef unsigned long long  clock_t;    /* time in CLOCKS_PER_SEC ticks */

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    
    int tm_mday;
    int tm_mon;
    int tm_year;

    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

typedef  struct tm  ymd_hms;

err_t time_ymd_from_rtc(ymd_hms *ymd);


time_t time(time_t *t);
clock_t clock(void);

struct tm *gmtime(const time_t *timep);
struct tm *localtime(const time_t *timep);
time_t mktime(struct tm *tm);

size_t strftime(char *s, size_t max, const char *format, const struct tm *tm);

double difftime(time_t time1, time_t time0);

#endif //__TIME_H__

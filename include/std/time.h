#ifndef __TIME_H__
#define __TIME_H__

typedef unsigned long time_t;

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

#endif //__TIME_H__

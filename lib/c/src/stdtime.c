#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include <cosec/log.h>

#warning "TODO: difftime, gmtime, localtime, strftime, clock"
#pragma GCC diagnostic ignored "-Wunused"
#pragma GCC diagnostic ignored "-Wunused-parameter"

double difftime(time_t time1, time_t time0) {
    return (double)time1 - (double)time0;
}

struct tm *gmtime(const time_t *timep) {
    logmsge("TODO: gmtime");
    return NULL;
}

struct tm *localtime(const time_t *timep) {
    logmsge("TODO: localtime");
    return NULL;
}

size_t strftime(char *s, size_t max, const char *format, const struct tm *tm) {
    logmsge("TODO: strftime");
    return 0;
}

/*
 *  Posix epoch calculation
 */
static time_t epoch_by_year[100] = {
    /* 1970 */ 0,          31536000,   63072000,   94694400,
    /* 1974 */ 126230400,  157766400,  189302400,  220924800,
    /* 1978 */ 252460800,  283996800,  315532800,  347155200,
    /* 1982 */ 378691200,  410227200,  441763200,  473385600,
    /* 1986 */ 504921600,  536457600,  567993600,  599616000,
    /* 1990 */ 631152000,  662688000,  694224000,  725846400,
    /* 1994 */ 757382400,  788918400,  820454400,  852076800,
    /* 1998 */ 883612800,  915148800,  946684800,  978307200,
    /* 2002 */ 1009843200, 1041379200, 1072915200, 1104537600,
    /* 2006 */ 1136073600, 1167609600, 1199145600, 1230768000,
    /* 2010 */ 1262304000, 1293840000, 1325376000, 1356998400,
    /* 2014 */ 1388534400, 1420070400, 1451606400, 1483228800,
    /* 2018 */ 1514764800, 1546300800, 1577836800, 1609459200,
    /* 2022 */ 1640995200, 1672531200, 1704067200, 1735689600,
    /* 2026 */ 1767225600, 1798761600, 1830297600, 1861920000,
    /* 2030 */ 1893456000, 1924992000, 1956528000, 1988150400,
    /* 2034 */ 2019686400, 2051222400, 2082758400, 2114380800,
    /* 2038 */ 2145916800, 2177452800, 2208988800, 2240611200,
    /* 2042 */ 2272147200, 2303683200, 2335219200, 2366841600,
    /* 2046 */ 2398377600, 2429913600, 2461449600, 2493072000,
    /* 2050 */ 2524608000, 2556144000, 2587680000, 2619302400,
    /* 2054 */ 2650838400, 2682374400, 2713910400, 2745532800,
    /* 2058 */ 2777068800, 2808604800, 2840140800, 2871763200,
    /* 2062 */ 2903299200, 2934835200, 2966371200, 2997993600,
    /* 2066 */ 3029529600, 3061065600, 3092601600, 3124224000,
};

static int cumul_month_days[13] = {
    365, 0,  31,  59,
         90, 120, 151,
         181, 212, 243,
         273, 304, 334
};
static int cumul_month_days_with_leap[13] = {
    366, 0,  31,  60,
         91, 121, 152,
         182, 213, 244,
         274, 305, 335
};

static inline bool is_leap_year(int year) {
    if (year % 4) return false;
    if (year % 400 == 0) return true;
    return (bool)(year % 100);
}

time_t mktime(struct tm *tm) {
    bool has_leap = is_leap_year(tm->tm_year);
    int tm_year = tm->tm_year - 1970;
    time_t epoch = epoch_by_year[tm_year];

    int days = (has_leap ? cumul_month_days_with_leap : cumul_month_days)[tm->tm_mon];
    days += (tm->tm_mday - 1);
    epoch += days * 86400;

    epoch += tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec;
    return epoch;
}

#include <stdlib.h>
#include <time.h>

#include <cosec/log.h>

#warning "TODO: difftime, gmtime, localtime, strftime, clock"
#pragma GCC diagnostic ignored "-Wunused"
#pragma GCC diagnostic ignored "-Wunused-parameter"

double difftime(time_t time1, time_t time0) {
    return (double)time1 - (double)time0;
}

time_t mktime(struct tm *tm) {
    logmsge("TODO: mktime");
    return 0;
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

clock_t clock(void) {
    logmsge("TODO: clock");
    return 0;
}

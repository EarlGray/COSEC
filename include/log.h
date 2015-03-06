#ifndef __KERN_LOG_H__
#define __KERN_LOG_H__

#include <stdarg.h>
#include <dev/screen.h>
#include <sys/errno.h>

/*
 *  kernel log
 */
int logging_setup();
int vlprintf(const char *fmt, va_list ap);
int lprintf(const char *fmt, ...);

void k_printf(const char *fmt, ...);

#define logmsg(msg) lprintf(msg)
#define logmsgf(...) lprintf(__VA_ARGS__)

#define logmsge(...) do { \
    logmsg("Error: "); logmsgf(__VA_ARGS__); logmsg("\n"); \
    k_printf("Error: "); k_printf(__VA_ARGS__); k_printf("\n"); \
} while (0)
#define logmsgef(...)  logmsge(__VA_ARGS__)

#define logmsgi(...) do { \
    logmsg("### "); logmsgf(__VA_ARGS__); logmsg("\n"); \
    k_printf("### "); k_printf(__VA_ARGS__); k_printf("\n"); \
} while (0)
#define logmsgif(...)  logmsgi(__VA_ARGS__)

#ifdef __DEBUG
#   define logmsgdf(...) logmsgf(__VA_ARGS__)
#else
#   define logmsgd(msg)
#   define logmsgdf(...)
#endif

#define returnv_dbg_if(assertion, ...) \
    if (assertion) {  logmsgdf(__VA_ARGS__); return; }

#define return_dbg_if(assertion, retval, ...) \
    if (assertion) {  logmsgdf(__VA_ARGS__); return (retval); }

#define returnv_log_if(assertion, ...) \
    if (assertion) {  logmsgf(__VA_ARGS__); return; }

#define return_log_if(assertion, retval, ...) \
    if (assertion) {  logmsgf(__VA_ARGS__); return (retval); }

#define returnv_err_if(assertion, ...) \
    if (assertion) {  logmsgef(__VA_ARGS__); return; }

#define return_err_if(assertion, retval, ...) \
    if (assertion) {  logmsgef(__VA_ARGS__); return (retval); }

#define assert(assertion, retval, ...)    \
    if (!(assertion)) { logmsgef(__VA_ARGS__); return (retval); }

#define assertv(assertion, ...)    \
    if (!(assertion)) { logmsgef(__VA_ARGS__); return; }


extern void panic(const char *fatal_error);

#endif  //__KERN_LOG_H__

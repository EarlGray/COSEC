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

#define logmsge(...) do { logmsg("Error: "); logmsgf(__VA_ARGS__); logmsg("\n"); } while (0)
#define logmsgef(...)  logmsge(__VA_ARGS__)

#ifdef __DEBUG
#   define logmsgd(msg) logmsg(msg)
#   define logmsgdf(...) logmsgf(__VA_ARGS__)
#else
#   define logmsgd(msg)
#   define logmsgdf(...)
#endif

#define return_if_eq(expr1, ret_expr) \
    if ((expr1) == (ret_expr)) return (ret_expr);

#define return_if(assertion, retval, msg) \
    if (assertion) {  logmsgd(msg); return (retval); }

#define returnv_if(assertion, msg) \
    if (assertion) {  logmsgd(msg); return; }

#define returnf_if(assertion, retval, fmt, ...) \
    if (assertion) {  logmsgdf(msg, __VA_ARGS__); return (retval); }

#define assertq(assertion, retval) \
    if (!(assertion)) { return (retval); }

#define assert(assertion, retval, ...)    \
    if (!(assertion)) { logmsgef(__VA_ARGS__); return (retval); }

#define assertv(assertion, ...)    \
    if (!(assertion)) { logmsgef(__VA_ARGS__); return; }


extern void panic(const char *fatal_error);

#endif  //__KERN_LOG_H__

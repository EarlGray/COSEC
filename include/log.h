#ifndef __KERN_LOG_H__
#define __KERN_LOG_H__

#include <dev/screen.h>
#include <std/sys/errno.h>

#define print(msg) k_printf(msg)
#define printf(msg, ...) k_printf(msg, __VA_ARGS__)

#define log(msg) k_printf(msg)
#define logf(msg, ...) k_printf((msg), __VA_ARGS__)

#define loge(msg) logf("Error: %s", (msg))
#define logef(fmt, ...) do { log("Error: "); logf((fmt), __VA_ARGS__); } while (0)

#ifdef __DEBUG
#   define logd(msg) log(msg)
#   define logdf(fmt, ...) logf((fmt), __VA_ARGS__)
#else
#   define logd(msg)
#   define logdf(fmt, ...)
#endif

#define return_if_eq(expr1, ret_expr) \
    if ((expr1) == (ret_expr)) return (ret_expr);

#define return_if(assertion, retval, msg) \
    if (assertion) {  logd(msg); return (retval); }

#define returnv_if(assertion, msg) \
    if (assertion) {  logd(msg); return; }

#define returnf_if(assertion, retval, fmt, ...) \
    if (assertion) {  logdf(msg, __VA_ARGS__); return (retval); }

#define assertq(assertion, retval) \
    if (!(assertion)) { return (retval); }

#define assert(assertion, retval, msg) \
    if (!(assertion)) { logd(msg); return (retval); }

#define assertf(assertion, retval, fmt, ...)    \
    if (!(assertion)) { logdf((fmt), __VA_ARGS__); return (retval); }

#define assertv(assertion, fmt)    \
    if (!(assertion)) { logd(fmt); return; }

#define assertvf(assertion, fmt, ...)    \
    if (!(assertion)) { logdf((fmt), __VA_ARGS__); return; }


extern void panic(const char *fatal_error);

#endif  //__KERN_LOG_H__

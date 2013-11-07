#ifndef __KERN_LOG_H__
#define __KERN_LOG_H__

#include <dev/screen.h>
#include <std/sys/errno.h>

#define print(msg) k_printf(msg)
#define printf(...) k_printf(__VA_ARGS__)

#define log(msg) k_printf(msg)
#define logf(...) k_printf(__VA_ARGS__)

#define loge(...) do { log("Error: "); logf(__VA_ARGS__); log("\n"); } while (0)
#define logef(...)  loge(__VA_ARGS__)

#ifdef __DEBUG
#   define logd(msg) log(msg)
#   define logdf(...) logf(__VA_ARGS__)
#else
#   define logd(msg)
#   define logdf(...)
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

#define assert(assertion, retval, ...)    \
    if (!(assertion)) { logef(__VA_ARGS__); return (retval); }

#define assertv(assertion, ...)    \
    if (!(assertion)) { logef(__VA_ARGS__); return; }


extern void panic(const char *fatal_error);

#endif  //__KERN_LOG_H__

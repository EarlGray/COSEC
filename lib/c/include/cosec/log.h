#ifndef __KERN_LOG_H__
#define __KERN_LOG_H__

#include <stdarg.h>

/*
 *  kernel log
 */
#ifdef COSEC_KERN
int logging_setup();

int k_printf(const char *fmt, ...);

#else
# define k_printf(...)
#endif

int lprintf(const char *fmt, ...);
int vlprintf(const char *fmt, va_list ap);


#define logmsg(msg) lprintf(msg)
#define logmsgf(...) lprintf(__VA_ARGS__)

#if defined(COSEC) || defined(COSEC_KERN)
#define logmsge(...) do { \
    logmsg("Error: "); logmsgf(__VA_ARGS__); logmsg("\n"); \
    k_printf("Error: "); k_printf(__VA_ARGS__); k_printf("\n"); \
} while (0)
#else
# include <stdio.h>
# define logmsge(...)  fprintf(stderr, __VA_ARGS__)
#endif

#define logmsgef(...)  logmsge(__VA_ARGS__)

#define logmsgi(...) do { \
    logmsg("### "); logmsgf(__VA_ARGS__); logmsg("\n"); \
    k_printf("### "); k_printf(__VA_ARGS__); k_printf("\n"); \
} while (0)
#define logmsgif(...)  logmsgi(__VA_ARGS__)

#ifdef __DEBUG
#   define logmsgdf(...) logmsgf(__VA_ARGS__)
#   define logmsgd(msg) logmsgf(msg)
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

#define returnv_msg_if(assertion, ...) \
    if (assertion) {  logmsgif(__VA_ARGS__); return; }

#define return_msg_if(assertion, retval, ...) \
    if (assertion) {  logmsgif(__VA_ARGS__); return (retval); }

#define assert(assertion, retval, ...)    \
    if (!(assertion)) { logmsgef(__VA_ARGS__); return (retval); }

#define assertv(assertion, ...)    \
    if (!(assertion)) { logmsgef(__VA_ARGS__); return; }


extern void panic(const char *fatal_error);

#endif  //__KERN_LOG_H__

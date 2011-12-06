#ifndef __KERN_LOG_H__
#define __KERN_LOG_H__

#include <dev/screen.h>

#define print(msg) k_printf(msg)
#define printf(msg, ...) k_printf(msg, __VA_ARGS__)

#define log(msg) k_printf(msg)
#define logf(msg, ...) k_printf((msg), __VA_ARGS__)

extern void panic(const char *fatal_error);

#endif  //__KERN_LOG_H__

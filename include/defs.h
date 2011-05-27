#ifndef __MISC_H
#define __MISC_H

#define VERBOSE

typedef unsigned char	    uint8_t;
typedef unsigned short	    uint16_t;
typedef unsigned int	    uint32_t, uint20_t, uint;
typedef unsigned long long  uint64_t;

typedef unsigned int	size_t;

typedef char bool;
#define true 1
#define false 0
#define not(b) (1-(b))

#define null ((void *)0)

extern void* memset(void *s, int c, size_t n);
extern void k_printf(const char *fmt, ...);
extern void panic(const char *fatal_error);

#endif // __MISC_H

/**********************************************************/
#ifndef __CONF_H__
#define __CONF_H__

#define VERBOSE

#endif // __CONF_H__

/**********************************************************/
#ifndef __LANGEXT_H__
#define __LANGEXT_H__

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

#endif // __LANGEXT_H__

/**********************************************************/
#ifndef __GLOB_SYMS_H__
#define __GLOB_SYMS_H__

extern void* memset(void *s, int c, size_t n);
extern void k_printf(const char *fmt, ...);
extern void panic(const char *fatal_error);

#endif // __GLOB_SYMS_H__

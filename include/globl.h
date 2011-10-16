/**********************************************************/
#ifndef __CONF_H__
#define __CONF_H__

#define VERBOSE
#define VIRTUAL_OFFSET  (0xc0000000)

#define INTR_PROFILING  (0)
#define MEM_DEBUG       (1)
#define TASK_DEBUG      (0)

#endif // __CONF_H__

#ifndef ASM

/**********************************************************/
#ifndef __LANGEXTS__
#define __LANGEXTS__

typedef unsigned char	    uint8_t;
typedef unsigned short	    uint16_t;
typedef unsigned int	    uint32_t, uint20_t, uint;
typedef unsigned long long  uint64_t;

typedef short               int16_t;
typedef long                int32_t;
typedef long long           int64_t;

typedef uint32_t	size_t, ssize_t, err_t, ptr_t, index_t, count_t;

typedef char bool;
#define true 1
#define false 0
#define not(b) (1-(b))

#define null ((void *)0)

#define reinterpret_cast(t)        *(t *)&

#endif // __LANGEXTS__

/**********************************************************/
#ifndef __GLOB_SYMS_H__
#define __GLOB_SYMS_H__

extern void* memset(void *s, int c, size_t n);
extern void k_printf(const char *fmt, ...);
extern void panic(const char *fatal_error);

extern void* kmalloc(size_t);
extern void kfree(void *);

#endif // __GLOB_SYMS_H__

#endif // ASM

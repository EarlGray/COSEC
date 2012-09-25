/**********************************************************/
#include <conf.h>

#ifndef NOT_CC

/**********************************************************/
#ifndef __LANGEXTS__
#define __LANGEXTS__

/*
 *  primitive types
 */
typedef unsigned char	    uint8_t, byte;
typedef unsigned short	    uint16_t;
typedef unsigned int	    uint32_t, uint20_t, uint;
typedef unsigned long long  uint64_t, ulong;

typedef short               int16_t;
typedef long                int32_t;
typedef long long           int64_t;

typedef uint32_t	size_t, ssize_t, ptr_t, index_t, count_t, mode_t;
typedef int32_t     err_t, off_t;

#define MAX_UINT    (0xFFFFFFFF)
#define MAX_INT    (0x7FFFFFFF)
#define MAX_ULONG   (i(MAX_UINT << 32) | MAX_UINT)

#define NOERR               0

/*
 *  bool type
 */
typedef char bool;
#define true 1
#define false 0
#define not(b) (1-(b))

/*
 *  memory routines and definitions
 */
#define null ((void *)0)

#define reinterpret_cast(t)         *(t *)&

#define for_range(var, max_value)   for(var = 0; var < (max_value); ++var)

/*
 *  compiler-specific: only gcc now
 */
#define __pure      __attribute__((pure))
#define __noreturn  __attribute__((noreturn))

#endif // __LANGEXTS__

#ifndef __KSTDLIB_H__
#define __KSTDLIB_H__

extern void *kmalloc(size_t);
extern int kfree(void *);

#define tmalloc(_type)  ((_type *) kmalloc(sizeof(_type)))

#endif //__KSTDLIB_H__

#endif // NOTCC

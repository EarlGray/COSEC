/**********************************************************/
#ifndef __CONF_H__
#define __CONF_H__

#define PAGE_SIZE       0x1000

#define PAGING          (0)

#define INTR_PROFILING  (0)
#define MEM_DEBUG       (1)
#define TASK_DEBUG      (0)
#define INTR_DEBUG      (1)

#if PAGING
# define KERN_OFF       0xc0000000
#else 
# define KERN_OFF       0x00000000
#endif

#define KERN_PA         0x00100000

#endif // __CONF_H__

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

#endif // __LANGEXTS__

#ifndef __KSTDLIB_H__
#define __KSTDLIB_H__

extern void *kmalloc(size_t);
extern int kfree(void *);

#define tmalloc(_type)  ((_type *) kmalloc(sizeof(_type)))

#endif //__KSTDLIB_H__

#endif // NOTCC

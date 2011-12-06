/**********************************************************/
#ifndef __CONF_H__
#define __CONF_H__

#define VERBOSE
#define VIRTUAL_OFFSET  (0xc0000000)

#define INTR_PROFILING  (0)
#define MEM_DEBUG       (1)
#define TASK_DEBUG      (0)
#define INTR_DEBUG      (1)

#endif // __CONF_H__

#ifndef ASM

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

typedef uint	size_t, ssize_t, err_t, ptr_t, index_t, count_t;

#define MAX_UINT    (0xFFFFFFFF)
#define MAX_ULONG   (i(MAX_UINT << 32) | MAX_UINT)

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

#define reinterpret_cast(t)        *(t *)&

/*
 *  Lists
 */
#define __list

#define DLINKED_LIST        \
    struct {                \
        void *next, *prev;  \
    } link;

#define list_next(node) ((typeof node *)(node->link.next))
#define list_prev(node) ((typeof node *)(node->link.prev))

#endif // __LANGEXTS__

#endif // ASM

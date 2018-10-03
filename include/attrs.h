/**********************************************************/
#include <conf.h>

/**********************************************************/
#ifndef __LANGEXTS__
#define __LANGEXTS__

/*
 *  compiler-specific: only gcc now
 */
#ifdef __GNUC__
# define __constf    __attribute__((const))
# define __pure      __attribute__((pure))
# define __noreturn  __attribute__((noreturn))
# define __alloc     __attribute__((malloc))
# define __unused    __attribute__((unused))

# define __align(x)  __attribute__((align(x)))
# define __packed    __attribute__((packed))

# define likely(x)   __builtin_expect(!!(x), 1)
# define unlikely(x) __builtin_expect(!!(x), 0)
# define FALLTHROUGH __attribute__((fallthrough))
#else
# define __constf
# define __pure
# define __noreturn
# define __alloc
# define __unused

# define __align(x)
# define __packed
# define likely(x)
# define unlikely(x)
# define FALLTHROUGH
#endif

#endif // __LANGEXTS__


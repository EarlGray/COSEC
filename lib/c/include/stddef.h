#ifndef __COSEC_LIBC_STDDEF__
#define __COSEC_LIBC_STDDEF__

#define BUFSIZ 4096

#include <stdlib.h>

#define offsetof(type, member) \
    (size_t)(&(((type *)NULL)->member))

typedef int           ptrdiff_t;
typedef unsigned int  size_t;

#endif // __COSEC_LIBC_STDDEF__

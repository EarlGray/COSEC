#ifndef __COSEC_STDINT_H__
#define __COSEC_STDINT_H__

#include <sys/types.h>

typedef unsigned char       uint8_t, byte;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t, uint;
typedef unsigned long long  uint64_t, ulong;

typedef short               int16_t;
typedef long                int32_t;
typedef long long           int64_t;

typedef uint32_t    size_t, uintptr_t, index_t, count_t, mode_t;
typedef int32_t     ssize_t, intptr_t, err_t;
typedef index_t     kdev_t;

#endif //__COSEC_STDINT_H__

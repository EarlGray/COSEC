#ifndef __MEMDEVS_H__
#define __MEMDEVS_H__

#define MEMDEV_PMEM     1
#define MEMDEV_NULL     3
#define MEMDEV_PORT     4
#define MEMDEV_ZERO     5
#define MEMDEV_FULL     7
#define MEMDEV_RAND     8

#include <dev/devtable.h>

extern devfile_driver_t memdevs_driver;

#endif //__MEMDEVS_H__

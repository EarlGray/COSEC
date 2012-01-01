#ifndef __DEVTABLE_H__
#define __DEVTABLE_H__

#include <std/list.h>
#include <std/sys/types.h>

#define MAX_MAJOR           10

#define MEM_DEVS_FAMILY     1
#define TTY_DEVS_FAMILY     4

struct devfile_driver_struct;
struct devfile_t;
struct inode_operations;

typedef  struct devfile_driver_struct  devfile_driver_t;
typedef  struct devfile_struct         devfile_t;
typedef  struct inode_operations        inode_ops;

struct devfile_driver_struct {
    int major;

    inode_ops *ops;
    __list__ devfile_t *minors;
};

struct devfile_struct {
    int minor;
    devfile_driver_t *driver;

    DLINKED_LIST
};

bool dev_is_valid(dev_t dev);
int devtable_setup(void);
int dev_register(int major, devfile_driver_t *dev);

#endif //__DEVTABLE_H__

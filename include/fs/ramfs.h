#ifndef __COSEC_RAMFS_H__
#define __COSEC_RAMFS_H__

#include <fs/vfs.h>

/* ASCII "RAM" */
#define RAMFS_ID  0x004d4152

fsdriver * ramfs_fs_driver(void);

#endif //__COSEC_RAMFS_H__

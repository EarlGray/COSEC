#ifndef __RAMFS_H__
#define __RAMFS_H__

#include <vfs.h>

extern inode_ops ramfs_inode_ops;

const filesystem_t *get_ramfs(void);

#endif //__RAMFS_H__

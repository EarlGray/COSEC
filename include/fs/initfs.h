#ifndef __INIT_FS_H__
#define __INIT_FS_H__

struct filesystem;

err_t fs_load_initfs(struct filesystem *rootfs);

#endif // __INIT_FS_H__

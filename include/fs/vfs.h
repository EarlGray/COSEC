#ifndef __VFS_H__
#define __VFS_H__

#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>

void print_ls(const char *path);
void print_mount(void);
void vfs_setup(void);

typedef struct mount_opts_t  mount_opts_t;

struct mount_opts_t {
    uint fs_id;
    bool readonly:1;
};

int vfs_mount(dev_t source, const char *target, const mount_opts_t *opts);
int vfs_mkdir(const char *path, mode_t mode);
int vfs_mknod(const char *path, mode_t mode, dev_t dev);
  
#endif // __VFS_H__

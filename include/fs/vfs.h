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

err_t vfs_mount(dev_t source, const char *target, const mount_opts_t *opts);

#endif // __VFS_H__

#include <fs/vfs.h>
#include <std/sys/errno.h>

psuperblock_t *theSuperblocks;

err_t vfs_mount(const char *source, const char *target, const mount_opts_t *opts) {
    return -ETODO;
}

err_t vfs_mkdir(const char *path, mode_t mode) {
    return -ETODO;
}

void print_ls(const char *path) {
    printf("TODO: VFS not implemented\n");
}

void print_mount(void) {
    printf("TODO: VFS not implemented\n");
}

void vfs_shell(const char *cmd) {
    printf("TODO: VFS not implemented\n");
}

void vfs_setup(void) {
    vfs_mount("elffs", "/binary", null);
}

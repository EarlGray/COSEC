#include <fs/vfs.h>
#include <std/sys/errno.h>

psuperblock_t *theSuperblocks;

err_t vfs_mount(const char *source, const char *target, const mount_opts_t *opts) {
    return ETODO;
}

err_t vfs_mkdir(const char *path, mode_t mode) {
    return ETODO;
}

void print_ls(const char *path) {
}

void print_mount(void) {
}

void vfs_shell(const char *cmd) {
}

void vfs_setup(void) {
}

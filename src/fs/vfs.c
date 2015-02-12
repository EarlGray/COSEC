#include <stdlib.h>
#include <sys/errno.h>

#include <log.h>

#include <fs/vfs.h>

psuperblock_t *theSuperblocks;

err_t vfs_mount(const char *source, const char *target, const mount_opts_t *opts) {
    return -ETODO;
}

err_t vfs_mkdir(const char *path, mode_t mode) {
    return -ETODO;
}

void print_ls(const char *path) {
    logmsgef("TODO: VFS not implemented\n");
}

void print_mount(void) {
    logmsgef("TODO: VFS not implemented\n");
}

void vfs_shell(const char *cmd) {
    logmsgef("TODO: VFS not implemented\n");
}

void vfs_setup(void) {
    int ret;
    ret = vfs_mount("ramfs", "/", null);
    assertv(ret == 0, "root mount on ramfs failed\n");
    k_printf("ramfs on / mounted successfully");

    ret = vfs_mkdir("/binary", 0);
    assertv(ret == 0, "mkdir /binary failed");

    ret = vfs_mount("elffs", "/binary", null);
    assertv(ret == 0, "mount of elffs on /binary failed");
}

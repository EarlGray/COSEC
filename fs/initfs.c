#include <fs/initfs.h>

const uint initfs_image = {
#include <fs/initfs.inc>
};

err_t fs_load_initfs(struct filesystem *rootfs) {
    // now initfs is compiled into kernel
    return 0;
}



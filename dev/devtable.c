#include <dev/devtable.h>
#include <std/sys/errno.h>
#include <std/sys/types.h>
#include <vfs.h>
#include <log.h>

#include <dev/memdevs.h>

devfile_driver_t * theDeviceTable[MAX_MAJOR + 1] = {
    [0] = null,
    [MEM_DEVS_FAMILY] = &memdevs_driver,
};

int dev_register(int major, devfile_driver_t *dev) {
    assert ((0 > major) || (major > MAX_MAJOR), EINVAL,
            "dev_register(): invalid dev family");
    assert (null == theDeviceTable[major], EEXIST,
            "dev_register(): already taken number");
    theDeviceTable[major] = dev;
    return 0;
}

bool dev_is_valid(dev_t dev) {
    int major = major(dev);
    assertq( (0 < major) && (major <= MAX_MAJOR), false);
    assertq( theDeviceTable[major], false);
    return true;
}

err_t dev_init_inode(inode_t *inode, dev_t dev) {
    // @TODO
    return ETODO;
}

int devtable_setup(void) {
}

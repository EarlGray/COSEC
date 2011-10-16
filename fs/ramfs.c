#include <vfs.h>

static char *ramfs_name_for_device(const char *dev) {
    char *devname = kmalloc(strlen(dev) + 1);
    strcpy(devname, dev);
    return devname;
};

filesystem_t ramfs = {
    .name = "ramfs",
    .get_name_for_device = ramfs_name_for_device,
    .populate_tree = null,
};

#include <dev/memdevs.h>
#include <dev/devtable.h>
#include <std/sys/types.h>
#include <vfs.h>
#include <log.h>

static ssize_t memdev_read(inode_t *, void *, off_t, size_t);
static ssize_t memdev_write(inode_t *, void *, off_t, size_t);

inode_ops memdevs_ops = {
    .read = memdev_read,
    .write = memdev_write,
};

devfile_driver_t memdevs_driver = {
    .major = MEM_DEVS_FAMILY,
    .ops = &memdevs_ops,

    .minors = null,
};

static ssize_t memdev_read(inode_t *inode, void *buf, off_t offset, size_t count) {
    
}

static ssize_t memdev_write(inode_t *inode, void *buf, off_t offset, size_t count) {
}



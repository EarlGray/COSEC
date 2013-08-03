#include <fs/devfs.h>

#include <dev/devices.h>

#include <log.h>

static int devfs_get_inode(inode_t *ino);
static int devfs_put_inode(inode_t *ino);
static int devfs_statfs(superblock_t *sb, struct statvfs *stat);

static fs_ops devfs_ops = {
    .get_inode = devfs_get_inode,
    .put_inode = devfs_put_inode,
    .statfs = devfs_statfs,
};

pfsdriver_t devfs = {
    .fsd = {
        .name = "devfs",
        .ops = &devfs_ops,
    },
};

static int devfs_get_inode(inode_t *ino) {
    // fill inode for device with dev_id == ino->no;
    
}

static int devfs_put_inode(inode_t *ino) {
    // try to apply changes in ino to dev_id == ino->no
}

static int devfs_statfs(superblock_t *sb, struct statvfs *stat) {
    // get vfs stat
}

#include <fs/devfs.h>

#include <dev/table.h>

#include <log.h>

static int devfs_read_inode(superblock_t *sb, inode_t *ino);
static int devfs_write_inode(superblock_t *sb, inode_t *ino);
static int devfs_statfs(superblock_t *sb, struct statvfs *stat);

static fs_ops devfs_ops = {
    .read_inode = devfs_read_inode,
    .write_inode = devfs_write_inode,
    .statfs = devfs_statfs,
};

pfsdriver_t devfs = {
    .fsd = {
        .name = "devfs",
        .ops = &devfs_ops,
    },
};

static int devfs_read_inode(superblock_t *sb, inode_t *ino) {
    // fill inode for device with dev_id == ino->no;
    
}

static int devfs_write_inode(superblock_t *sb, inode_t *ino) {
    // try to apply changes in ino to dev_id == ino->no
}

static int devfs_statfs(superblock_t *sb, struct statvfs *stat) {
    // get vfs stat
}

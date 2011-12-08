#include <vfs.h>
#include <std/string.h>

/*
 *      RAMFS
 *  Ramfs is
 *
 */

#define RAMFS_BLOCK_SIZE    512     /* size of ramfs block in bytes */
#define RAMFS_POOL_SIZE     32      /* size of ramfs pool in pages */

#define BLOCKS_PER_POOL     ((RAMFS_POOL_SIZE * PAGE_SIZE) / RAMFS_BLOCK_SIZE)

typedef struct {
    index_t next;
} ramfs_block_t;

typedef struct {
    inode_t inode;
    size_t file_size_bytes;
    index_t first_block;
} ramfs_inode_t;


static char *ramfs_name_for_device(const char *dev) {
    char *devname = strcpy(null, dev);
    return devname;
};

filesystem_t ramfs = {
    .name = "ramfs",
    .get_name_for_device = ramfs_name_for_device,
    .grow_branch = null,
};

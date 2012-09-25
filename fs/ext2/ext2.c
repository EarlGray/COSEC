#include "ext2.h"

#include <dev/block.h>

#define EXT2_SUPERBLOCK_OFFSET 1024

int e2fs_init(ext2fs_t *fs, block_dev_t* dev) {
    memset(fs, 0, sizeof(struct ext2fs));

    fs->bd = dev;   

    char *blockbuf = 
    dev->ops->read_block(dev, (dev->blksz > 1024)? 0 : 1, );

    return 0;
}

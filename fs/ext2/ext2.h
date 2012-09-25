#ifndef __EXT2_H__
#define __EXT2_H__

#include <linux/ext2_fs.h>

typedef struct ext2fs {
    struct ext2_super_block sb;
    block_dev_t* bd;
} ext2fs_t;

int e2fs_init(ext2fs *fs, block_device* dev);

#endif // __EXT2_H__


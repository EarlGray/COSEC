#include "ext2.h"

#include <dev/block.h>

#define EXT2_SUPERBLOCK_OFFSET 1024

int e2fs_init(ext2fs_t *fs, block_dev_t* dev) {
    memset(fs, 0, sizeof(struct ext2fs));

    fs->bd = dev;
    
    dev->ops->read_block(dev, (dev->blksz > 1024)? 0 : 1, );

    return 0;
}  

void print_ext2(ext2fs_t *fs, const char *cmd) {
    if (!strncmp(cmd, "sb", 2)) {
        assertvf(fs->sb->s_magic != EXT2_SUPER_MAGIC, "ext2 magic (0x%x) is wrong\n", fs->sb->s_magic);

        k_printf("Superblock info (%s):\n", (fs->sb->s_state != EXT2_VALID_FS ? "dirty" : "clean"));
        k_printf("  block size: %d\n", fs->blksz);
        k_printf("  inodes count = %d (free: %d)\n", 
                fs->sb->s_inodes_count, fs->sb->s_free_inodes_count);
        k_printf("  blocks count = %d (free: %d)\n", 
                fs->sb->s_blocks_count, fs->sb->s_free_blocks_count);
        k_printf("  blocks per group: %d, inodes per group: %d\n",
                fs->sb->s_blocks_per_group, fs->sb->s_inodes_per_group);
    } else {
        k_printf("Options: sb");
    }
}

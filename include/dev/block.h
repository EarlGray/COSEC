#ifndef __BLOCK_DEV_H__
#define __BLOCK_DEV_H__

typedef struct block_ops {
    int (*read_block)(block_device*, index_t, char *);
    int (*write_block)(block_device*, index_t, const char *);
} block_ops;

typedef struct block_dev_t {
    uint blksz;
    block_ops* ops;
    cache_t *cache;
} block_dev_t;

#endif // __BLOCK_DEV_H__

#include <vfs.h>
#include <std/string.h>

/*
 *      RAMFS
 *  Ramfs is a piece of memory which contains blocks of files
 *  and block descriptors array:
 *      Ramfs pool:
 *  |----|----|----|- ... -|----|
 *  <---> <-----            ---->
 *  bl.arr.  blocks with file contents
 *
 *  Size of pool is determined as count of blocks descriptors
 *  which fit to a one block; the first block is used for holding
 *  the block descriptor's array; the firts item of this array
 *  points to first free block; each block descriptor holds index
 *  of next block:
 *      - if block is free, it's list of free blocks,
 *      - if block is used, it points to next block of the file,
 *      - if block is the end of file, it points to EOF_BLOCK
 *  If free blocks in a pool are exausted, it can be linked to
 *  a next pool. A ramfs structure is tracking and managing pools.
 */


/* size of ramfs block in bytes */
#define RAMFS_DEF_BLOCK_SIZE    512

#define RAMFS_EOF_BLOCK         MAX_UINT
#define RAMFS_FREE_BLOCK        0


static ssize_t ramfs_read(inode_t *this, void *buf, size_t count);
static ssize_t ramfs_write(inode_t *this, void *buf, size_t count);

static mnode_t *
ramfs_new_superblock(const char *source, mount_options opts);

static inode_t *ramfs_new_inode(mnode_t *this);


/*
 *     the inode operations structure
 */
inode_ops ramfs_inode_ops = {
   .read = ramfs_read,
   .write = ramfs_write,
};

/*
 *     the filesystem structure
 */
filesystem_t ramfs = {
    .name = "ramfs",

    /* fs methods */
    .new_superblock = ramfs_new_superblock,
    .new_inode = ramfs_new_inode,
};



/*
 *      Internal declarations
 */

struct ramfs_superblock_struct;
typedef  struct ramfs_superblock_struct  ramfs_superblock;

typedef struct {
    /* indeces are individual for the whole pool list,
       this pool contains
       [index_offset : index_offset + n_blocks - 1]
       see below:  pool_by_index(ramfs_superblock *, index_t);
    */
    index_t index_offset;

    count_t n_free_blocks;

    void *mem;

    DLINKED_LIST
} ramfs_pool_t;


typedef struct {
    /* if this block is free, it's next free block,
       if this block contains a part of a file, it's index
        of the next block */
    index_t next;
} ramfs_block_descriptor_t;


#define BLK_DESCR_SIZE  sizeof(ramfs_block_descriptor_t)

/* allocate and insert into 'this' new pool */
static ramfs_pool_t *make_pool(ramfs_superblock *this);

/* find block memory by index */
static void *block_by_index(const ramfs_superblock *this, index_t index);

/* find block descriptor by index */
static ramfs_block_descriptor_t *
    blk_descr_by_index(const ramfs_superblock *this, index_t index);

/*
 *      VFS structures extensions
 */
struct ramfs_superblock_struct {
    /* vfs base superblock */
    mnode_t vfs;

    /* blocks per pool */
    count_t blocks_per_pool;

    /* ordered by pools.index_offset */
    __list ramfs_pool_t *pools;
};

static inline size_t _block_size(const ramfs_superblock *);

typedef struct {
    inode_t vfs;    /* vfs index node */

    index_t start_block;
} ramfs_inode_t;


/*
 *      fs operations
 */
static mnode_t *ramfs_new_superblock(
        const char *source,
        mount_options opts) {
    ramfs_superblock *sb = tmalloc(ramfs_superblock);

    mnode_t *mnode = (mnode_t *)sb;

    // TODO: read block_size from 'opts'
    mnode->blk.block_size = RAMFS_DEF_BLOCK_SIZE;

    mnode->name = strcpy(null, source);

    /** initialize a pool **/
    // how many memory?
    sb->blocks_per_pool =
        mnode->blk.block_size / BLK_DESCR_SIZE;
    if (mnode->blk.block_size % BLK_DESCR_SIZE)
        ++ sb->blocks_per_pool;

    void *new_pool = make_pool(sb);
    if (new_pool == null) {
        kfree(mnode->name);
        kfree(sb);
        return null;
    }

    mnode->inodes = null;

    return mnode;
}


static inode_t *ramfs_new_inode(mnode_t *this) {
    ramfs_inode_t *inode = tmalloc(ramfs_inode_t);
    inode->vfs.length = 0;
    inode->vfs.ops = &ramfs_inode_ops;

    /* insert into fs inode list */
}


/*
 *    inode operations
 */
static ssize_t ramfs_read(
        inode_t *this, void *buf, size_t count) {
    return 0;
}

static ssize_t ramfs_write(
        inode_t *this, void *buf, size_t count) {
    return 0;
}


/*
 *      Internal routines
 */

static inline size_t _block_size(const ramfs_superblock *sb) {
    return sb->vfs.blk.block_size;
}

static inline ramfs_pool_t *
pool_by_index(const ramfs_superblock *this, index_t index) {
    ramfs_pool_t *pool;
    list_foreach(pool, this->pools) {
        if ((pool->index_offset <= index)
           && (index < (pool->index_offset + this->blocks_per_pool)))
            return pool;
    }
    return null;
}

static void *
block_by_index(const ramfs_superblock *this, index_t index) {
    ramfs_pool_t *pool = pool_by_index(this, index);

    if (! pool) return null;

    size_t byte_offset =
        (index - pool->index_offset) * _block_size(this);

    return ((uint8_t *)pool->mem ) + byte_offset;
}

static ramfs_block_descriptor_t *
blk_descr_by_index(const ramfs_superblock *this, index_t index) {
    ramfs_pool_t *pool = pool_by_index(this, index);

    if (! pool) return null;
    size_t byte_offset =
        (index - pool->index_offset) * BLK_DESCR_SIZE;

    return ((uint8_t)pool->mem) + byte_offset;
}


static ramfs_pool_t *make_pool(ramfs_superblock *this) {
    size_t pool_size =
        this->blocks_per_pool * _block_size(this);

    size_t n_pages = pool_size/PAGE_SIZE;
    if (pool_size % PAGE_SIZE)
        ++n_pages;

    void *mem = (void *) PAGE_SIZE * pmem_alloc(n_pages);
    if (null == mem)
        return null;

    ramfs_pool_t *pool = tmalloc(ramfs_pool_t);
    if (pool == null)
        return null;

    pool->mem = mem;
    pool->n_free_blocks = this->blocks_per_pool;
    pool->index_offset = ;

}



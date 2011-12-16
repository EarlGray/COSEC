#include <vfs.h>

#include <fs/ramfs.h>

#include <pmem.h>

#include <std/string.h>

/*
 *      RAMFS
 *  Ramfs is a filesystem in RAM. It is an extensible FS.
 *  For each mounted RAMFS there's a list of pools.
 *  Ramfs pool is a piece of memory which contains array of blocks
 *  and metainformation structure and block descriptors array
 *  in the first block:
 *      Ramfs pool:
 *  |----|----|----|- ... -|----|
 *  <---> <-----            ---->
 *  bl.arr.  blocks with file contents
 *
 *  Size of pool is determined as count of blocks descriptors
 *  which fit to one block.
 *  Block descriptor (blkdescr_t) contains pointer to
 *  its pool, pointer to block itself and pointer to next block descrip-
 *  tor. The first block in pool is used for holding the block
 *  descriptors array; the first item of this array
 *  points to first free block; each block descriptor holds index
 *  of next block:
 *      - if block is free, it's list of free blocks,
 *      - if block is used, it points to next block of the file,
 *      - if block is the end of file, it points to EOF_BLOCK
 *  If free blocks in a pool are exausted, it can be linked to
 *  a next pool. A ramfs structure is tracking and managing pools,
 *  allocating pool if there are no free blocks;
 *
 *  Ramfs does not track directories, it contains only blocks of some
 *  content, directory/links handling is up to VFS. It extends VFS's
 *  structures, adding specific information, like pointer to start block
 *  for each inode. Memory management of superblocks and inodes is RAMFS
 *  duty, whereas it does not know anything about dnodes (Linux's
 *  dentries) and file links.
 */


/* size of ramfs block in bytes */
#define RAMFS_DEF_BLOCK_SIZE    2048

#define RAMFS_EOF_BLOCK         MAX_UINT
#define RAMFS_FREE_BLOCK        0


static ssize_t ramfs_read(inode_t *this, void *buf, off_t offset, size_t count);
static ssize_t ramfs_write(inode_t *this, void *buf, off_t offset, size_t count);

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

const filesystem_t *get_ramfs(void) {
    return &ramfs;
}


/*
 *      Internal declarations
 */

struct ramfs_blkdescr_struct;
struct ramfs_superblock_struct;
struct ramfs_pool_struct;

typedef void * block_t;

typedef  struct ramfs_blkdescr_struct  blkdescr_t;
typedef  struct ramfs_superblock_struct  ramfs_superblock_t;
typedef  struct ramfs_pool_struct  ramfs_pool_t;


struct ramfs_blkdescr_struct {
    /* pool of this block */
    ramfs_pool_t *pool;

    /* pointer to data */
    block_t data;

    /* if this block is free, it's next free block,
       if this block contains a part of a file, it's index
        of the next block */
    DLINKED_LIST
};


typedef struct {
    count_t n_blocks;       /* a copy of sb->blocks_per_pool */
    count_t n_free_blocks;

    size_t block_size;      /* a copy of sb->vfs.blk.block_size */

    DLINKED_LIST            /* pools list for superblock */
} ramfs_pool_info_t;

struct ramfs_pool_struct {
    /* header with metainfo */
    ramfs_pool_info_t inf;

    /* array of block descriptors for this pool.
       it fills the first block of the pool,
       its length is held in inf.n_blocks */
    blkdescr_t blkarray[0];
};

#define BLK_DESCR_SIZE  sizeof(blkdescr_t)

/* allocate and insert into 'this' a new pool */
static ramfs_pool_t *make_pool(ramfs_superblock_t *this);

static inline blkdescr_t *
get_free_blocks_of(ramfs_pool_t *pool) {
    return pool->blkarray + RAMFS_FREE_BLOCK;
}

static inline ptr_t
block_aligned(const blkdescr_t *b, ptr_t addr) {
    ptr_t blksz = b->pool->inf.block_size;
    return addr / blksz + ( addr % blksz ? blksz : 0 );
}

static __list blkdescr_t *
allocate_block_list(ramfs_superblock_t *sb, size_t block_count);

static err_t free_block_list(__list blkdescr_t *blocks);


/*
 *      VFS structures extensions
 */
struct ramfs_superblock_struct {
    /* vfs base superblock */
    mnode_t vfs;

    /* blocks per pool */
    count_t blocks_per_pool;

    /* pools */
    __list ramfs_pool_t *pools;
};


typedef struct {
    inode_t vfs;    /* vfs index node */

    blkdescr_t* start_block;
} ramfs_inode_t;


/*
 *      fs operations
 */
static mnode_t *ramfs_new_superblock(
        const char *source,
        mount_options opts) {
    ramfs_superblock_t *sb = tmalloc(ramfs_superblock_t);

    mnode_t *mnode = (mnode_t *)sb;

    // TODO: read block_size from 'opts'
    mnode->blk.block_size = RAMFS_DEF_BLOCK_SIZE;

    mnode->name = strdup(source);

    mnode->inodes = null;
    mnode->ops = &ramfs_inode_ops;

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

    return mnode;
}


/** create a new ramfs_inode without content
  with least possible index
  and insert it to 'this->inodes */
static inode_t *ramfs_new_inode(mnode_t *this) {
    ramfs_inode_t *inode = tmalloc(ramfs_inode_t);
    inode_t *vfsinode = (inode_t *) inode;

    vfsinode->length = 0;
    vfsinode->ops = &ramfs_inode_ops;

    /* insert into fs inode list,
       keeping list sorted by inode->index */
    index_t index = 0;

    // search for free index
    inode_t *cur_inode = list_head(this->inodes);
    if (!empty_list(cur_inode))
        list_foreach (cur_inode, this->inodes) {
            inode_t *next_inode = list_next(cur_inode);
            if (empty_list(next_inode)) {
                // the end reached, insert here
                list_insert_after(cur_inode, vfsinode);
                break;
            }

            if ((next_inode->index - cur_inode->index) > 1) {
                // an index gap found, fill it
                index = cur_inode->index + 1;
                list_insert_between(vfsinode, cur_inode, next_inode);
                break;
            }
        }
    else // no inodes in list at the moment
        list_init(this->inodes, cur_inode);

    vfsinode->index = index;
    inode->start_block = null;
    return vfsinode;
}


/*
 *    inode operations
 */
static ssize_t ramfs_read(
        inode_t *this, void *buf, off_t offset, size_t count) {
    ramfs_inode_t *rfs_inode = (ramfs_inode_t *)this;

    size_t bytes_read = 0;
    off_t offp = offset;

    size_t bytes_to_copy;

    blkdescr_t * block = rfs_inode->start_block;

    const size_t blksz = block->pool->inf.block_size;

    while (offset - offp > (int)blksz) {
        offp += blksz;
        block = list_next(block);
    }

    /* read blocks */
    while ((offset + bytes_read < this->length) && (bytes_read < count)) {
        bytes_to_copy = blksz - (offp % blksz);
        if (offp + bytes_to_copy > this->length)
            bytes_to_copy = this->length - offp;

        memcpy(buf, (uint8_t *)(block->data) + offp % blksz, bytes_to_copy);

        bytes_read += bytes_to_copy;
        offp += bytes_to_copy;
        buf = (uint8_t *)buf + bytes_to_copy;
        block = list_next(block);
    }

    return bytes_read;
}

static ssize_t ramfs_write(
        inode_t *this, void *buf, off_t offset, size_t count) {
    ramfs_inode_t *rthis = (ramfs_inode_t *)this;

    // is there enough place?
    const size_t blksz = block_size(this->mnode);
    size_t blocks_count = this->length / blksz;
    if (this->length % blksz) ++blocks_count;

    size_t blocks_needed = (offset + count) / blksz;
    if ((offset + count) % blksz) ++blocks_needed;

    int n_newblocks = (int)blocks_needed - (int)blocks_count;
    if (n_newblocks > 0) {
        // allocate additional blocks
        ramfs_superblock_t *sb = (ramfs_superblock_t *)this->mnode;
        __list blkdescr_t *newl = allocate_block_list(sb, n_newblocks);
        list_append(rthis->start_block, newl);
    }

    size_t bytes_written = 0;
    size_t bytes_to_write = 0;
    off_t offp = 0;

    blkdescr_t *block = rthis->start_block;
    while (offp + blksz < offset) {
        offp += blksz;
        block = list_next(block);
    }

    while (bytes_written < count) {
        const block_offset = offp % blksz;
        bytes_to_write = blksz - block_offset;
        if (bytes_to_write > count)
            bytes_to_write = count;

        memcpy((uint8_t*)(block->data) + block_offset, 
                buf, bytes_to_write);

        bytes_written += bytes_to_write;
        offp += bytes_to_write;
        buf = (uint8_t *)buf + bytes_to_write;
        block = list_next(block);
    }

    if (offp > this->length)
        this->length = offp;

    return bytes_written;
}


/*
 *      Internal routines
 */


/** return and initialize a new pool with free blocks
     This procedure does not link pool with others!
 **/
static ramfs_pool_t *new_pool(ramfs_superblock_t *this) {
    size_t pool_size =
        this->blocks_per_pool * this->vfs.blk.block_size;

    size_t n_pages = pool_size/PAGE_SIZE;
    if (pool_size % PAGE_SIZE)
        ++n_pages;

    ramfs_pool_t *pool = (void *)( PAGE_SIZE * pmem_alloc(n_pages) );
    if (null == pool)
        return null;

    pool->inf.n_blocks = this->blocks_per_pool;     // with metadata block
    pool->inf.n_free_blocks = pool->inf.n_blocks - 1; // the first block is already taken
    pool->inf.block_size = block_size((mnode_t *)this);

    // initialize blocks as free
    blkdescr_t *blkdescr = pool->blkarray;

    index_t i = 0;
    for (; i < pool->inf.n_blocks; ++i) {
        blkdescr->pool = pool;
        blkdescr->data = (block_t) ( (ptr_t)pool + pool->inf.block_size * i );
        list_link_prev(blkdescr, blkdescr - 1);
        list_link_next(blkdescr, blkdescr + 1);

        ++blkdescr;
    }

    list_link_prev(pool->blkarray, null);
    list_link_next(blkdescr, null);

    return pool;
}

/** create a new pool and insert it into superblock's pools **/
static ramfs_pool_t *make_pool(ramfs_superblock_t *this) {
    ramfs_pool_t *pool = new_pool(this);
    if (pool == null)
        return null;

    ramfs_pool_info_t *poolinf = (ramfs_pool_info_t *)pool;

    if (empty_list(this->pools)) {
        ramfs_pool_info_t *this_pools_inf =
            (ramfs_pool_info_t *)(this->pools);
        list_init(this_pools_inf, poolinf);
        return pool;
    }

    /** else: append this pool to pools **/
    ramfs_pool_info_t *last_pool;
    list_last(this->pools, last_pool);

    /* concatenate lists of free blocks */
    blkdescr_t *free_block;
    list_last(get_free_blocks_of((ramfs_pool_t *)last_pool),
            free_block);

    list_link_prev( get_free_blocks_of(pool), free_block);
    list_link_next( free_block, get_free_blocks_of(pool));

    /* concatenate pools at last */
    list_insert_after(last_pool, poolinf);

    return pool;
}



/*
 *      Blocks management
 */

static blkdescr_t *
allocate_block(ramfs_superblock_t *sb) {
    blkdescr_t *blkdescr = null;

    list_foreach (blkdescr, get_free_blocks_of(sb->pools)) {
        /* the first block of a pool can not be taken */
        if (blkdescr != blkdescr->pool->blkarray) {
            /* release blkdescr from list of free blocks */
            list_node_release(blkdescr);
            -- blkdescr->pool->inf.n_free_blocks;
            break;
        }
    }

    return blkdescr;
}

static __list blkdescr_t *
allocate_block_list(ramfs_superblock_t *sb, size_t block_count) {
    typedef blkdescr_t bd_t;

    __list bd_t *newlist = null;

    // can not use list_foreach() here due to list modification
    bd_t *blkdescr;
    bd_t *next_bd = get_free_blocks_of(sb->pools);

    while (!empty_list(blkdescr = next_bd))
    {
        /* get next node before 'blkdescr' can be unmerged from list */
        next_bd = list_next(blkdescr);

        /* the first block of a pool can not be taken */
        if (blkdescr != blkdescr->pool->blkarray) {
            /* release blkdescr from list of free blocks */
            list_node_release(blkdescr);
            list_append(newlist, blkdescr);

            -- blkdescr->pool->inf.n_free_blocks;
            -- block_count;

            if (block_count == 0)
                return newlist;
        }

        if (empty_list(next_bd)) {
            if ( make_pool(sb) )  /* pools are consumed, need more place */
                /* try again with additional pool */
                next_bd = list_next(blkdescr);
        }
    }

    return null;
}

static err_t free_block_list(__list blkdescr_t *blocks) {
    typedef blkdescr_t bd_t;

    bd_t *cur_blk;
    bd_t *next_blk = blocks;

    while (!empty_list( cur_blk = next_blk )) {
        next_blk = list_next(cur_blk);

        __list bd_t *free_blocks_of_pool = get_free_blocks_of(cur_blk->pool);

        list_insert_between(cur_blk,
                list_head(free_blocks_of_pool),
                list_next(free_blocks_of_pool));

        cur_blk->pool->inf.n_free_blocks ++;
    }

    return 0;
}

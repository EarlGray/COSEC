#ifndef __VIRTUAL_FILE_SYSTEM__
#define __VIRTUAL_FILE_SYSTEM__

#define list

struct file_link;

struct directory;
struct superblock;
struct index_node;
struct inode_operations;

struct vfs_pool_struct;
struct filesystem_struct;

typedef  struct index_node          inode_t;
typedef  struct inode_operations    inode_ops;
typedef  struct file_link           flink_t;
typedef  struct directory           dnode_t;
typedef  struct filesystem_struct   filesystem_t;
typedef  struct superblock          mnode_t;
typedef  struct mount_options_struct  mount_options;
typedef  struct vfs_pool_struct     vfs_pool_t;

/***
  *         File structure
 ***/

/** inode types **/
#define IT_FILE     '-'
#define IT_DIR      'd'
#define IT_CHARDEV  'c'

typedef char filetype_t;

/*
 *  Interface for file data access,
 *  does not know anything about file content and structure
 */
struct index_node {
    mnode_t* mnode;
    index_t index;

    count_t nlink;  /* reference count */

    size_t length;  /* content length */

    inode_ops *ops; /* inode operations */

    DLINKED_LIST    /* index list */
};

struct inode_operations {
    ssize_t (*read)(inode_t *, void *, off_t, size_t);
    ssize_t (*write)(inode_t *, void *, off_t, size_t);
};

/***
  *         Directory structure
 ***/

#define FS_DELIM      '/'

/*
 *  determines file type,
 *  links to physical inode
 *  holds file name,
 *  owns additional file metainformation.
 */
struct file_link {
    /* who I am */
    filetype_t type;

    /* what is my name */
    const char *f_name;

    /* society (directory) I belong to  */
    dnode_t *f_dir;

    /* my body */
    inode_t *f_inode;

    /* my social responsibilities according to 'type',
       e.g. 'dnode_t *' for directory */
    void *type_spec;

    /* circle of acquaintances (my directory) */
    DLINKED_LIST
};


static inline filetype_t vfs_flink_type(flink_t *flink) {
    return flink->type;
}

static inline bool vfs_is_directory(flink_t *flink) {
    return flink->type == IT_DIR;
}

static inline dnode_t *get_dnode(flink_t *flink) {
    if (!vfs_is_directory(flink)) return null;
    return (dnode_t *) flink->type_spec;
}


/*
 *  directory entry
 */
struct directory {
    /* directory content starting from "." and ".." */
    __list flink_t *d_files;

    flink_t *d_file;     /* which file this directory is */
    dnode_t *d_parent;   /* shortcut for d_files[1], ".." */
    mnode_t *d_mount;    /* pointer to mounted fs */

    DLINKED_LIST         /* list of directories node */
};

err_t vfs_mkdir(const char *path, uint mode);

bool vfs_is_node(inode_t *node, uint node_type);


/***
  *         Mount structure
 ***/

/* An abstract filesystem */
struct filesystem_struct {
    const char *name;

    mnode_t* (*new_superblock)(const char *source, mount_options *opts);

    inode_t* (*new_inode)(mnode_t *);
};

filesystem_t* fs_by_name(const char *);
err_t fs_register(filesystem_t *fs);

/* helper structure for a block device */
typedef struct {
    size_t block_size;  /* 0, if a chardev */
} blockdev_t;

/* Superblock structure
 * Note: Since superblocks are allocated with fs->new_sb(),
 * we don't need this ugly void* fs_specific here.
 */
struct superblock {
    /** contain name for a specific device, e.g. "MyDisk" **/
    char *name;

    /** fs driver **/
    filesystem_t *fs;

    /** mount point for this mount node **/
    dnode_t *at;

    /** count of dependent mount nodes **/
    count_t n_deps;

    /** inodes list
        Note: this field should be readonly, because filesystem
        must manage inodes and their memory.
     **/
    __list inode_t *inodes;

    /** operations **/
    inode_ops *ops;

    /** block device info **/
    blockdev_t blk;
};


static inline size_t block_size(const mnode_t *superblock) {
    return superblock->blk.block_size;
}

static inline bool is_block_device(mnode_t *superblock) {
    return superblock->blk.block_size != 0;
}

/** mount flags **/
#define MS_DEFAULT  0

/** mount errors **/
#define EINVAL      0x8BAD
#define ENOMEM      0xFFFF

#define ENODEV      0x8001 /* fs type is not recognized */
#define ENOENT      0x8002
#define ENOTDIR     0x8003
#define EEXIST      0x8004
#define EBUSY       0x8005

struct mount_options_struct {
    const char *fs_str;     /* name of file system type, e.g. "ramfs" */
    uint flags;
};


/** Mount: file 'what' at 'at' as 'fs' **/
err_t vfs_mount(const char *what, const char *at, const char *fs);

/** Mount with options **/
err_t vfs_opt_mount(const char *what, const char *at, mount_options *mntopts);

/***
  *         File structures
 ***/

/** File descriptors **/
typedef uint file_t;

/** Struct encapsulating all file resources relating to e.g. process **/
struct vfs_pool_struct {
    dnode_t *cwd;
};

/*** **************************************************************************
  *     File operations
 ***/

/** Try to open file by name
err_t vfs_open(const char *fname, int flags);

ssize_t vfs_read(file_t fd, void *buf, size_t count);
ssize_t vfs_write(file_t fd, void *buf, size_t count);

err_t vfs_close(file_t fd);
**/


/*** **************************************************************************
  *     Directory operations
 ***/

void print_ls(const char *);
void print_mount(void);

void vfs_shell(const char *arg);
void vfs_setup(void);

#endif // __VIRTUAL_FILE_SYSTEM__

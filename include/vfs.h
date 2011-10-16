#ifndef __VIRTUAL_FILE_SYSTEM__
#define __VIRTUAL_FILE_SYSTEM__

#define list

struct file_link;

struct directory;
struct mount_node;
struct index_node;

struct vfs_pool_struct;
struct filesystem_struct;

typedef  struct filesystem_struct   filesystem_t;
typedef  struct mount_node          mnode_t;
typedef  struct index_node          inode_t;
typedef  struct directory           dnode_t;
typedef  struct vfs_pool_struct     vfs_pool_t;
typedef  struct file_link           flink_t;


/***
  *         File structure
 ***/

/** inode types **/
#define IT_FILE     1
#define IT_DIR      2
#define IT_CHARDEV  3


struct index_node {
    count_t hlinks_count;
};


/***
  *         Directory structure
 ***/

#define FS_DELIM      '/'

struct file_link {
    const char *name;
    inode_t *inode;

    /** neighbours in directory **/
    flink_t *next_file;
    flink_t *prev_file;
};

struct directory {
    mnode_t *mnode;
    inode_t *inode;

    dnode_t *parent;

    list dnode_t *subdirs;
    list flink_t *files;

    /*  */
    dnode_t *next_dir;
    dnode_t *prev_dir;
};

int vfs_mkdir(const char *path, uint mode);

bool vfs_is_node(inode_t *node, uint node_type);


/***
  *         Mount structure
 ***/

struct filesystem_struct {
    const char *name;

    char * (*get_name_for_device)(const char *);
    int (*populate_tree)(mnode_t *);
};

filesystem_t* fs_by_name(const char *);
err_t fs_register(filesystem_t *fs);

struct mount_node {
    /** contain name for a specific device, e.g. "MyDisk" **/
    const char *name;

    /** fs driver **/
    filesystem_t *fs;

    /** mount point for this mount node **/
    dnode_t *at;

    /** count of dependent mount nodes **/
    count_t deps_count;
};


/** mount flags **/
#define MS_DEFAULT  0

/** mount errors **/
#define EINVAL      0x0001
#define ENOMEM      0x0002

#define ENODEV      0x8001
#define ENOENT      0x8002
#define ENOTDIR     0x8003

struct mount_options_struct {
    const char *fs_str;     /* name of file system type, e.g. "ramfs" */
    uint flags;
};
typedef  struct mount_options_struct  mount_options;


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

/** Try to open file by name **/
err_t vfs_open(const char *fname, int flags);

ssize_t vfs_read(file_t fd, void *buf, size_t count);
ssize_t vfs_write(file_t fd, void *buf, size_t count);

err_t vfs_close(file_t fd);



/*** **************************************************************************
  *     Directory operations
 ***/


void vfs_setup(void);

#endif // __VIRTUAL_FILE_SYSTEM__

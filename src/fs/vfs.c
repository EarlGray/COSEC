#include <stdlib.h>
#include <stdbool.h>
#include <sys/errno.h>

#include <log.h>

typedef uint16_t uid_t, gid_t;

typedef struct superblock superblock_t;
typedef struct fs_ops fs_ops;
typedef struct inode inode_t;
typedef struct fsdriver fsdriver_t;

typedef  struct mount_opts_t  mount_opts_t;

#define PINODE_SIZE 128

struct inode {
    kdev_t dev;         // device
    index_t no;           // ino index
    
    uid_t uid;
    gid_t gid;
    mode_t mode;

    size_t size;
    count_t nlinks;
};

typedef struct pinode {
    inode_t i;
    char info[0];
    char pad[PINODE_SIZE - sizeof(inode_t)];
} pinode_t; // padded inode

struct inode_ops {
    int (*read)(inode_t *, off_t, char *, size_t);          // read data
    int (*write)(inode_t *, off_t, const char *, size_t);   // write data

    int (*truncate)(inode_t *, size_t);
};

struct mount_opts_t {

};

inode_t * vfs_get_inode(superblock_t *, index_t);
int vfs_put_inode(inode_t *ino);
int vfs_release_inode(inode_t *ino);

#define PFSDRV_SIZE 256

struct fsdriver {
    const char *name;
    fs_ops *ops;
};

typedef struct pfsdriver {
    fsdriver_t fsd;
    char info[0];
    char pad[PFSDRV_SIZE - sizeof(fsdriver_t)];
} pfsdriver_t;  // padded fsdriver

struct fs_ops {
    inode_t * (*new_inode)(superblock_t *); // create an inode
    int (*del_inode)();                   // delete an inode

    int (*get_inode)(inode_t *);        // fills the inode_t structure
    int (*put_inode)(inode_t *);        // updates the inode by data in inode_t 

    int (*statfs)(superblock_t *, struct statvfs *);
    superblock_t * (*mountfs)(kdev_t, mount_opts_t *);
};

#define PSB_SIZE    512

struct superblock {
    dev_t dev;
    size_t blksz;
    fsdriver_t *fs;
    struct {
        bool dirty :1 ;
        bool ro :1 ;
    } flags;
};

typedef struct psuperblock {
    superblock_t sb;
    char info[0];
    char pad[PSB_SIZE - sizeof(superblock_t)];
} psuperblock_t; // padded superblock



err_t vfs_mount(const char *source, const char *target, const mount_opts_t *opts);
err_t vfs_mkdir(const char *path, mode_t mode);

psuperblock_t *theSuperblocks;


err_t vfs_mount(const char *source, const char *target, const mount_opts_t *opts) {
    return -ETODO;
}

err_t vfs_mkdir(const char *path, mode_t mode) {
    return -ETODO;
}



void print_ls(const char *path) {
    logmsgef("TODO: VFS not implemented\n");
}

void print_mount(void) {
    logmsgef("TODO: VFS not implemented\n");
}

void vfs_setup(void) {
    int ret;
    ret = vfs_mount("sysfs", "/", null);
    assertv(ret == 0, "root mount on ramfs failed\n");
    k_printf("ramfs on / mounted successfully");
}

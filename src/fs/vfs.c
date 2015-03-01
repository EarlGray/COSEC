#include <stdlib.h>
#include <stdbool.h>
#include <sys/errno.h>
#include <sys/stat.h>

#include <dev/devices.h>
#include <log.h>

typedef uint16_t uid_t, gid_t;

typedef struct superblock superblock_t;
typedef struct fs_ops fs_ops;
typedef struct inode inode_t;
typedef struct fsdriver fsdriver_t;

typedef struct mount_opts_t  mount_opts_t;

err_t vfs_mount(dev_t source, const char *target, const mount_opts_t *opts);
err_t vfs_mkdir(const char *path, mode_t mode);



struct superblock {
    dev_t dev;
    fsdriver_t *fs;

    size_t blksz;
    struct {
        bool dirty :1 ;
        bool ro :1 ;
    } flags;

    const char *relmntpath;       /* path relative to the parent mountpoint */
    uint32_t mntpath_hash;        /* hash of this relmntpath */
    struct superblock *brother;   /* the next superblock in the list of childs of parent */
    struct superblock *parent;
    struct superblock *childs;    /* list of this superblock child blocks */
};

struct superblock *theRootMnt = NULL;



struct inode {
    index_t no;           // ino index
    size_t size;
    count_t nlinks;
};


struct mount_opts_t {
    uint fs_id;
    bool readonly:1;
};

struct fsdriver {
    const char *name;
    uint fs_id;
    fs_ops *ops;

    struct {
        fsdriver_t *prev, *next;
    } lst;
};



struct fs_ops {
    int (*read_superblock)(struct superblock *sb);
};

fsdriver_t * theFileSystems = NULL;

void vfs_register_filesystem(fsdriver_t *fs) {
    if (!theFileSystems) {
        theFileSystems = fs;
        fs->lst.next = fs->lst.prev = fs;
        return;
    }

    /* insert just before theFileSystems in circular list */
    fsdriver_t *lastfs = theFileSystems->lst.prev;
    fs->lst.next = theFileSystems;
    fs->lst.prev = lastfs;
    lastfs->lst.next = fs;
    theFileSystems->lst.prev = fs;
};

fsdriver_t *vfs_fs_by_id(uint fs_id) {
    fsdriver_t *fs = theFileSystems;
    if (!fs) return NULL;

    do {
        if (fs->fs_id == fs_id)
            return fs;
    } while ((fs = fs->lst.next) != theFileSystems);
    return NULL;
}


/*
 *  Sysfs
 */

static int sysfs_read_superblock(struct superblock *sb);

fs_ops sysfs_fsops = {
    .read_superblock = sysfs_read_superblock,
}; 

fsdriver_t sysfs_driver = {
    .name = "sysfs",
    .fs_id = 0x00535953, /* "SYS" */
    .ops = &sysfs_fsops,
};

static int sysfs_read_superblock(struct superblock *sb) {
    sb->dev = gnu_dev_makedev(CHR_VIRT, CHR0_SYSFS);
    return 0;
}


err_t vfs_mount(dev_t source, const char *target, const mount_opts_t *opts) {
    if (theRootMnt == NULL) {
        if ((target[0] != '/') || (target[1] != '\0')) {
             logmsgef("vfs_mount: mount('%s') with no root", target);
             return -1;
        }

        fsdriver_t *fs = vfs_fs_by_id(opts->fs_id);
        if (!fs) return -2;

        struct superblock *sb = kmalloc(sizeof(struct superblock));
        return_err_if(!sb, -3, "vfs_mount: malloc(superblock) failed");

        sb->fs = fs;
        int ret = fs->ops->read_superblock(sb);
        return_err_if(ret, -4, "vfs_mount: read_superblock failed (%d)", ret);

        sb->parent = NULL;
        sb->brother = NULL;
        sb->childs = NULL;
        sb->relmntpath = NULL;

        theRootMnt = sb;
        return 0;
    }
    logmsge("vfs_mount: TODO: non-root");
    return -110;
}

err_t vfs_mkdir(const char *path, mode_t mode) {
    return -ETODO;
}



void print_ls(const char *path) {
    logmsgef("TODO: ls not implemented\n");
}



void print_mount(void) {
    struct superblock *sb = theRootMnt;
    if (!sb) return;

    k_printf("%s on /\n", sb->fs->name);
    if (sb->childs) {
        logmsge("TODO: print child mounts");
    }
}

void vfs_setup(void) {
    int ret;

    /* register filesystems here */
    vfs_register_filesystem(&sysfs_driver);

    /* mount actual filesystems */
    dev_t sysfsdev = gnu_dev_makedev(CHR_VIRT, CHR0_SYSFS);
    mount_opts_t mntopts = { .fs_id = 0x00535953 };
    ret = vfs_mount(sysfsdev, "/", &mntopts);
    returnv_err_if(ret, "root mount on sysfs failed (%d)", ret);

    k_printf("sysfs on / mounted successfully\n");
}

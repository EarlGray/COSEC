#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <sys/errno.h>
#include <sys/dirent.h>
#include <sys/stat.h>

#include <conf.h>
#include <attrs.h>

#include <fs/vfs.h>
#include <fs/ramfs.h>
#include <fs/devices.h>

#define __DEBUG
#include <log.h>

static off_t vfs_inode_block_by_index(struct inode *idata, off_t bno, size_t blksz);
static int vfs_inode_get(mountnode *sb, inode_t ino, struct inode *idata);
static int vfs_path_dirname_len(const char *path);
static const char * vfs_match_mountpath(mountnode *parent_mnt, mountnode **match_mnt, const char *path);


/*
 *  Global state
 */

fsdriver * theFileSystems = NULL;

struct superblock *theRootMnt = NULL;

/* should be inode #0 */
struct inode theInvalidInode;


/*
 *  Sysfs
 */
#if 0
#define SYSFS_ID  0x00535953

static int sysfs_read_superblock(struct superblock *sb);
static int sysfs_make_directory(struct superblock *sb, const char *path, mode_t mode);
static int sysfs_get_direntry(struct superblock *sb, void **iter, struct dirent *dir);
static int sysfs_make_inode(struct superblock *sb, mode_t mode, void *info);

fs_ops sysfs_fsops = {
    .read_superblock = sysfs_read_superblock,
    .make_directory = sysfs_make_directory,
    .get_direntry = sysfs_get_direntry,
    .make_inode = sysfs_make_inode,
    .lookup_inode = sysfs_lookup_inode,
    .link_inode = sysfs_link_inode,
    .unlink_inode = sysfs_unlink_inode,
};

struct sysfs_data {
    struct dirent *rootdir,
};

fsdriver_t sysfs_driver = {
    .name = "sysfs",
    .fs_id = SYSFS_ID, /* "SYS" */
    .ops = &sysfs_fsops,
};

struct dirent sysfs_inode_table[] = {
};

static int sysfs_read_superblock(mountnode *sb) {
    sb->dev = gnu_dev_makedev(CHR_VIRT, CHR0_SYSFS);
    sb->data = { .rootdir =
    return 0;
}

static int sysfs_make_directory(mountnode *sb, inode_t *ino, const char *path, mode_t mode) {
    return -ETODO;
}

static int sysfs_get_direntry(mountnode *sb, inode_t ino, void **iter, struct dirent *dir) {
}
#endif




/*
 *  VFS operations
 */

void vfs_register_filesystem(fsdriver *fs) {
    if (!theFileSystems) {
        theFileSystems = fs;
        fs->lst.next = fs->lst.prev = fs;
        return;
    }

    /* insert just before theFileSystems in circular list */
    fsdriver *lastfs = theFileSystems->lst.prev;
    fs->lst.next = theFileSystems;
    fs->lst.prev = lastfs;
    lastfs->lst.next = fs;
    theFileSystems->lst.prev = fs;
};

fsdriver *vfs_filesystem_by_id(uint fs_id) {
    fsdriver *fs = theFileSystems;
    if (!fs) return NULL;

    do {
        if (fs->fs_id == fs_id)
            return fs;
    } while ((fs = fs->lst.next) != theFileSystems);
    return NULL;
}



static const char * vfs_match_mountpath(mountnode *parent_mnt, mountnode **match_mnt, const char *path) {
    mountnode *child_mnt = parent_mnt->sb_children;
    while (child_mnt) {
        const char *mountpath = child_mnt->sb_mntpath;
        size_t mountpath_len = strlen(mountpath);
        if (!strncmp(path, mountpath, mountpath_len)) {
            const char *nextpath = path + mountpath_len;
            if (nextpath[0] == '\0')
                break;

            while (nextpath[0] == FS_SEP)
                ++nextpath;

            if (match_mnt) *match_mnt = child_mnt;
            return nextpath;
        }
        child_mnt = child_mnt->sb_brother;
    }

    if (match_mnt) *match_mnt = parent_mnt;
    return NULL;
}

/*
 *   Determines the mountnode `path` resides on.
 *   Returns:
 *      its pointer through `*mntnode` (if not NULL);
 *      filesystem-specific part of `path` through `*relpath` (if not NULL);
 *
 */
int vfs_mountnode_by_path(const char *path, mountnode **mntnode, const char **relpath) {
    return_log_if(!theRootMnt, EBADF, "vfs_mountnode_by_path(): theRootMnt absent\n");

    return_log_if(!(path && *path), EINVAL, "vfs_mountnode_by_path(NULL or '')\n");
    return_log_if(path[0] != FS_SEP, EINVAL, "vfs_mountnode_by_path('%s'): requires the absolute path\n", path);
    ++path;

    mountnode *mnt = theRootMnt;
    mountnode *chld = NULL;
    while (true) {
        const char * nextpath = vfs_match_mountpath(mnt, &chld, path);
        if (!nextpath)
            break;

        mnt = chld;
        path = nextpath;
    }

    if (mntnode) *mntnode = mnt;
    if (relpath) *relpath = path;
    return 0;
}


static int vfs_path_dirname_len(const char *path) {
    if (!path) return -1;

    char *last_sep = strrchr(path, FS_SEP);
    if (!last_sep) return 0;

    while (last_sep[-1] == FS_SEP)
        --last_sep;
    return (int)(last_sep - path);
}


int vfs_mount(dev_t source, const char *target, const mount_opts_t *opts) {
    if (theRootMnt == NULL) {
        if ((target[0] != '/') || (target[1] != '\0')) {
             logmsgef("vfs_mount: mount('%s') with no root", target);
             return -1;
        }

        fsdriver *fs = vfs_filesystem_by_id(opts->fs_id);
        if (!fs) return -2;

        struct superblock *sb = kmalloc(sizeof(struct superblock));
        return_err_if(!sb, -3, "vfs_mount: kmalloc(superblock) failed");

        sb->sb_parent = NULL;
        sb->sb_brother = NULL;
        sb->sb_children = NULL;
        sb->sb_mntpath = "";
        sb->sb_mntpath_hash = 0;
        sb->sb_dev = source;
        sb->sb_fs = fs;

        int ret = fs->ops->read_superblock(sb);
        return_err_if(ret, -4, "vfs_mount: read_superblock failed (%d)", ret);

        theRootMnt = sb;
        return 0;
    }

    logmsge("vfs_mount: TODO: non-root");
    return ETODO;
}


int vfs_mkdir(const char *path, mode_t mode) {
    int ret;
    mountnode *sb;
    const char *localpath;

    mode &= ~S_IFMT;
    mode |= S_IFDIR;

    ret = vfs_mountnode_by_path(path, &sb, &localpath);
    return_err_if(ret, ENOENT, "mkdir: no filesystem for path '%s'\n", path);
    logmsgdf("vfs_mkdir: localpath = '%s'\n", localpath);

    return_log_if(!sb->sb_fs->ops->make_directory, EBADF,
                  "mkdir: no %s.make_directory()\n", sb->sb_fs->name);

    ret = sb->sb_fs->ops->make_directory(sb, NULL, localpath, mode);
    return_err_if(ret, ret, "mkdir: failed (%d)\n", ret);

    return 0;
}


static int vfs_inode_get(mountnode *sb, inode_t ino, struct inode *idata) {
    const char *funcname = "vfs_inode_get";
    int ret;
    return_log_if(!idata, EINVAL, "%s(NULL", funcname);
    return_dbg_if(!sb->sb_fs->ops->inode_data, ENOSYS,
                  "%s: no %s.inode_data()\n", funcname, sb->sb_fs->name);

    ret = sb->sb_fs->ops->inode_data(sb, ino, idata);
    return_dbg_if(ret, ret, "%s: %s.inode_data failed\n", funcname, sb->sb_fs->name);

    return 0;
}


int vfs_mknod(const char *path, mode_t mode, dev_t dev) {
    const char *funcname = "vfs_mknod";
    int ret;
    return_log_if(S_ISDIR(mode), EINVAL, "Error: %s(IFDIR), use vfs_mkdir\n", funcname);
    return_log_if(S_ISLNK(mode), EINVAL, "Error: %s(IFLNK), use vfs_symlink\n", funcname);
    if ((S_IFMT & mode) == 0)
        mode |= S_IFREG;

    mountnode *sb = NULL;
    const char *fspath = NULL;
    inode_t ino = 0, dirino = 0;

    /* get filesystem and fspath */
    ret = vfs_mountnode_by_path(path, &sb, &fspath);
    return_dbg_if(ret, ret, "%s(%s) failed(%d)\n", funcname, path, ret);

    return_dbg_if(!sb->sb_fs->ops->make_inode, ENOSYS,
            "%s: %s does not support .make_inode()\n", funcname, sb->sb_fs->name);
    return_dbg_if(!sb->sb_fs->ops->link_inode, ENOSYS,
            "%s: no %s.link_inode()\n", funcname, sb->sb_fs->name);
    return_dbg_if(!sb->sb_fs->ops->lookup_inode, ENOSYS,
            "%s: no %s.lookup_inode()\n", funcname, sb->sb_fs->name);

    /* get dirino */
    int dirnamelen = vfs_path_dirname_len(fspath);
    ret = sb->sb_fs->ops->lookup_inode(sb, &dirino, fspath, dirnamelen);
    return_dbg_if(ret, ret, "%s: no dirino for %s\n", funcname, fspath);

    struct inode idata;
    vfs_inode_get(sb, dirino, &idata);
    return_dbg_if(!S_ISDIR(idata.i_mode), ENOTDIR,
                 "%s: dirino=%d is not a directory\n", funcname, dirino);

    /* create the inode */
    void *mkinfo = NULL;
    if (S_ISCHR(mode) || S_ISBLK(mode))
        mkinfo = (void *)(size_t)dev;

    ret = sb->sb_fs->ops->make_inode(sb, &ino, mode, mkinfo);
    return_dbg_if(ret, ret,
            "%s(%s): %s.make_inode(mode=0x%x) failed\n", funcname, path, sb->sb_fs->name, mode);

    /* insert the inode */
    const char *de_name = fspath + dirnamelen;
    while (de_name[0] == FS_SEP) ++de_name;
    logmsgdf("%s: inserting ino=%d as '%s'\n", funcname, ino, de_name);

    ret = sb->sb_fs->ops->link_inode(sb, ino, dirino, de_name, SIZE_MAX);
    if (ret) {
        logmsgdf("%s: %s.link_inode() failed(%d)\n", funcname, sb->sb_fs->name, ret);
        if (sb->sb_fs->ops->free_inode)
            sb->sb_fs->ops->free_inode(sb, ino);
        return ret;
    }

    return 0;
}


int vfs_inode_read(
        mountnode *sb, inode_t ino, off_t pos,
        char *buf, size_t buflen, size_t *written)
{
    const char *funcname = "vfs_inode_read";
    int ret;
    return_err_if(!buf, EINVAL, "%s(NULL)", funcname);

    struct inode idata;

    return_dbg_if(!sb->sb_fs->ops->inode_data, ENOSYS,
            "%s: no %s.inode_data\n", funcname, sb->sb_fs->name);
    return_dbg_if(!sb->sb_fs->ops->read_inode, ENOSYS,
            "%s: no %s.read_inode\n", funcname, sb->sb_fs->name);

    ret = sb->sb_fs->ops->inode_data(sb, ino, &idata);
    return_dbg_if(ret, ret,
            "%s: %s.inode_data(%d) failed(%d)\n", funcname, sb->sb_fs->name, ino, ret);

    dev_t devno;
    switch (idata.i_mode & S_IFMT) {
        case S_IFCHR:
            devno = gnu_dev_makedev(idata.as.dev.maj, idata.as.dev.min);
            return cdev_blocking_read(devno, pos, buf, buflen, written);
        case S_IFBLK:
            devno = gnu_dev_makedev(idata.as.dev.maj, idata.as.dev.min);
            return bdev_blocking_read(devno, pos, buf, buflen, written);
        case S_IFSOCK:  logmsgef("%s(ino=%d -- SOCK): ETODO", funcname, ino); return ETODO;
        case S_IFIFO:   logmsgef("%s(ino=%d -- FIFO): ETODO", funcname, ino); return ETODO;
        case S_IFLNK:   /* TODO: read link path, read its inode */ return ETODO;
        case S_IFDIR:   logmsgdf("%s(ino=%d): EISDIR\n", funcname, ino); return EISDIR;
    }

    if (pos >= idata.i_size) {
        if (written) *written = 0;
        return 0;
    }

    return sb->sb_fs->ops->read_inode(sb, ino, pos, buf, buflen, written);
}

int vfs_inode_write(
        mountnode *sb, inode_t ino, off_t pos,
        const char *buf, size_t buflen, size_t *written)
{
    const char *funcname = "vfs_inode_write";
    int ret;
    return_err_if(!buf, EINVAL, "%s(NULL)", funcname);

    struct inode idata;

    return_dbg_if(!sb->sb_fs->ops->inode_data, ENOSYS,
            "%s: no %s.inode_data\n", funcname, sb->sb_fs->name);
    return_dbg_if(!sb->sb_fs->ops->write_inode, ENOSYS,
            "%s: no %s.write_inode\n", funcname, sb->sb_fs->name);

    ret = sb->sb_fs->ops->inode_data(sb, ino, &idata);
    return_dbg_if(ret, ret,
            "%s; %s.inode_data(%d) failed(%d)\n", funcname, sb->sb_fs->name, ino, ret);

    return_dbg_if(S_ISDIR(idata.i_mode), EISDIR, "%s(inode=%d): EISDIR\n", funcname, ino);

    return sb->sb_fs->ops->write_inode(sb, ino, pos, buf, buflen, written);
}


int vfs_inode_stat(mountnode *sb, inode_t ino, struct stat *stat) {
    const char *funcname = __FUNCTION__;
    int ret;
    return_log_if(!stat, EINVAL, "%s(NULL)\n", funcname);

    struct inode idata;

    return_dbg_if(!sb->sb_fs->ops->inode_data, ENOSYS,
             "%s: no %s.inode_data\n", funcname, sb->sb_fs->name);
    ret = sb->sb_fs->ops->inode_data(sb, ino, &idata);
    return_dbg_if(ret, ret,
            "%s: %s.inode_data() failed(%d)\n", funcname, sb->sb_fs->name, ret);

    memset(stat, 0, sizeof(struct stat));
    stat->st_dev = sb->sb_dev;
    stat->st_ino = ino;
    stat->st_mode = idata.i_mode;
    stat->st_nlink = idata.i_nlinks;
    stat->st_rdev = (S_ISCHR(idata.i_mode) || S_ISBLK(idata.i_mode) ?
                        gnu_dev_makedev(idata.as.dev.maj, idata.as.dev.min) : 0);
    stat->st_size = idata.i_size;
    return 0;
}

int vfs_stat(const char *path, struct stat *stat) {
    const char *funcname = "vfs_stat";
    int ret;

    mountnode *sb = NULL;
    const char *fspath;
    inode_t ino = 0;

    ret = vfs_mountnode_by_path(path, &sb, &fspath);
    return_dbg_if(ret, ret,
            "%s: no localpath and superblock for path '%s'\n", funcname, path);

    return_dbg_if(!sb->sb_fs->ops->lookup_inode, ENOSYS,
            "%s: no %s.lookup_inode\n", funcname, sb->sb_fs->name);
    ret = sb->sb_fs->ops->lookup_inode(sb, &ino, fspath, SIZE_MAX);
    return_dbg_if(ret, ret,
            "%s: %s.lookup_inode failed(%d)\n", funcname, sb->sb_fs->name, ret);

    return vfs_inode_stat(sb, ino, stat);
}


void print_ls(const char *path) {
    int ret;
    mountnode *sb;
    const char *localpath;
    void *iter;
    struct dirent de;
    inode_t ino = 0;

    ret = vfs_mountnode_by_path(path, &sb, &localpath);
    returnv_err_if(ret, "ls: path '%s' not found\n", path, ret);
    logmsgdf("print_ls: localpath = '%s' \n", localpath);

    ret = sb->sb_fs->ops->lookup_inode(sb, &ino, localpath, SIZE_MAX);
    returnv_err_if(ret, "no inode at '%s' (%d)\n", localpath, ret);
    logmsgdf("print_ls: ino = %d\n", ino);

    iter = NULL; /* must be NULL at the start of enumeration */
    do {
        ret = sb->sb_fs->ops->get_direntry(sb, ino, &iter, &de);
        returnv_err_if(ret, "ls error: %s", strerror(ret));

        k_printf("%d\t%s\n", de.d_ino, de.d_name);
    } while (iter);

    /* the end of enumeration */
    k_printf("\n");
}

void print_mount(void) {
    struct superblock *sb = theRootMnt;
    if (!sb) return;

    k_printf("%s on /\n", sb->sb_fs->name);
    if (sb->sb_children) {
        logmsge("TODO: print child mounts");
    }
}

void vfs_setup(void) {
    int ret;

    /* register filesystems here */
    vfs_register_filesystem(ramfs_fs_driver());

    /* mount actual filesystems */
    dev_t fsdev = gnu_dev_makedev(CHR_MEMDEV, CHRMEM_MEM);
    mount_opts_t mntopts = { .fs_id = RAMFS_ID };
    ret = vfs_mount(fsdev, "/", &mntopts);
    returnv_err_if(ret, "root mount on sysfs failed (%d)", ret);

    k_printf("%s on / mounted successfully\n", theRootMnt->sb_fs->name);
}

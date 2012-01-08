#define __DEBUG

#include <vfs.h>
#include <dev/devtable.h>
#include <std/list.h>
#include <std/string.h>
#include <std/sys/errno.h>
#include <std/sys/stat.h>
#include <log.h>

vfs_pool_t default_pool;
#define current_vfs_pool() (&default_pool)

extern dnode_t dummy_root_directory;

/*
 *      Root directory structures
 */
flink_t dummy_root_file = {
    .f_name = "/",                  /* const: the only file with / in name */
    .f_inode = null,                /* to be set on mounting rootfs */
    .f_dir = null,                  /* const: root file does not belong to any directory */
    .type = IT_DIR,                 /* const */
    .type_spec = &dummy_root_directory,   /* const */

    DLINKED_LIST_INIT(null, null)   /* const */
};

dnode_t dummy_root_directory = {
    .d_file = &dummy_root_file,     /* const */
    .d_files = null,
    .d_parent = null,               /* const */
    .d_mount = null,

    DLINKED_LIST_INIT(null, null)   /* const */
};

flink_t *root_file = &dummy_root_file;

static err_t new_directory(dnode_t *parent, const char *name, dnode_t **dir);

size_t vfs_writei(inode_t *node, void *buf, off_t offset, size_t length);
size_t vfs_readi(inode_t *node, void *buf, off_t offset, size_t length);

/*
 *    File paths and search
 */
flink_t* vfs_file_by_name(const dnode_t *dir, const char *fname) {
    assert( dir, null, "file_by_name(null)\n");
    flink_t *file;
    list_foreach (file, dir->d_files) {
        if (!strcmp(fname, file->f_name))
            return file;
    }
    return null;
}

flink_t* vfs_file_by_nname(const dnode_t *dir, const char *fname, size_t n) {
    assert( dir, null, "file_by_nname(null)\n");
    flink_t *file;
    list_foreach(file, dir->d_files) {
        if ('\0' == file->f_name[strnlen(fname, n)])
            if (!strncmp((void *)fname, (void *)file->f_name, n))
                return file;
    }
    return null;
}

flink_t* vfs_file_by_npath(const char *path, size_t n) {
    return_if_eq (path, null);

    /* prepare pathname lookup */
    return_if (path[0] != FS_DELIM , null,
            "TODO: fbnp(): relative path resolution\n");

    if ( (path[1] == 0) || (n < 2))
        return root_file;

    dnode_t *current = get_dnode(root_file);
    char *start = (char *)path, *end = (char *)path;  // position of subdir name

    while (1) {
        start = end + 1;
        assert( ((start - path) < (off_t)n), null, "fbnp(): out of path\n");

        end = strchr(start, FS_DELIM);
        if ((null == end) || ((end - path) >= (off_t)n))
            // FS_DELIM not found, path remains a stripped file name:
            return vfs_file_by_nname(current, start, n - (start - path));

        flink_t *subdir = vfs_file_by_nname(current, start, end - start);
        assertf (subdir, null, "fbnp(): No such directory: %s\n", start);

        current = get_dnode(subdir);
        assertf (current, null, "fbnp(): Cannot get dnode for %s\n", start);
    }
}

flink_t* vfs_file_by_path(const char *path) {
    return vfs_file_by_npath(path, MAX_INT);
}


mode_t filetype_to_mode(filetype_t type) {
    switch (type) {
    case IT_FILE:   return S_IFREG;
    case IT_DIR:    return S_IFDIR;
    case IT_CHRDEV: return S_IFCHR;
    case IT_BLKDEV: return S_IFBLK;
    default: return 0;
    }
}

/***
  *         Filesystems
 ***/

struct filesystems_list_t {
    filesystem_t *fs;

    DLINKED_LIST
};

struct filesystems_list_t *filesystems_list = null;

err_t fs_register(filesystem_t *fs) {
    assert( fs, EINVAL, "fs_register(null)");

    struct filesystems_list_t *entry =
        tmalloc(struct filesystems_list_t);
    if (null == entry) return ENOMEM;

    entry->fs = fs;
    list_insert(filesystems_list, entry);
    return 0;
}

filesystem_t * fs_by_name(const char *name) {
    struct filesystems_list_t *fslist;
    list_foreach(fslist, filesystems_list) {
        filesystem_t *fs = fslist->fs;
        if (!strcmp(fs->name, name))
            return fs;
    }
    return null;
};



struct mountpoints_list_t {
    mnode_t *mnode;
    dnode_t *shadow_dir;
    struct mountpoints_list_t *next;
};

struct mountpoints_list_t *mountpoints_list = null;

err_t vfs_is_dir_mountable(dnode_t *dir) {
    /* is 'dir' or any dir below a mountpoint */
    struct mountpoints_list_t *mntlist = mountpoints_list;
    for (; mntlist; mntlist = mntlist->next) {
        dnode_t *at_parent = mntlist->mnode->at;
        for (; at_parent; at_parent = at_parent->d_parent) {
            if (at_parent == dir)
                return EBUSY;
        }
    }
    return 0;
}

err_t mount(const char *source, dnode_t *dir, filesystem_t *fs, mount_options *opts) {
    struct mountpoints_list_t
        *new_mountlist_node = tmalloc(struct mountpoints_list_t);
    assertq (new_mountlist_node, ENOMEM);

    /* create superblock and initialize fs-dependent part */
    mnode_t *mnode = fs->new_superblock(source, null);
    assert (mnode, ENOMEM, "Cannot create superblock\n");

    /* initialize fs-independent fields */
    mnode->n_deps = 0;
    mnode->fs = fs;

    /* replace file link in dir->d_parent with new one */
    new_mountlist_node->shadow_dir = dir;
    if (dir->d_parent)
        list_release(dir->d_parent->d_files, dir->d_file);

    /* initialize mountpoint */
    dnode_t *mntdir;
    new_directory(dir->d_parent, null, &mntdir);
    mntdir->d_file->f_name = dir->d_file->f_name;
    mntdir->d_mount = mnode;

    mnode->at = mntdir;

    if (! dir->d_parent)
        root_file = mntdir->d_file;

    /* insert mnode to mountlist */
    new_mountlist_node->mnode = mnode;
    new_mountlist_node->next = mountpoints_list;
    mountpoints_list = new_mountlist_node;
    return 0;
};

static err_t check_device_mountability(const char *source, filesystem_t *fs) {
    //flink_t *

    return 0;
}

err_t vfs_mount(const char *source, const char *target, const char *fstype) {
    logf("mount('%s', '%s', '%s')\n", source, target, fstype);
    err_t reterr = 0;

    /* filesystem must be registered */
    filesystem_t *fs = fs_by_name(fstype);
    if (!fs) return ENODEV;

    /* target mountability */
    flink_t *target_flink = vfs_file_by_path(target);
    if (target_flink == null)
        return ENOENT;  /* target must be valid */

    dnode_t *dir = get_dnode(target_flink);
    if (null == dir)
        return ENOTDIR;

    reterr = vfs_is_dir_mountable(dir);
    if (reterr)
        return reterr;

    /* source must be mountable */
    reterr = check_device_mountability(source, fs);
    if (reterr)
        return reterr;

    return mount(source, dir, fs, null);
}


static inode_t *new_inode(mnode_t *sb, mode_t mode) {
    inode_t *node = sb->fs->new_inode(sb, mode);
    node->mnode = sb;
    node->nlink = 0;
    node->mode = mode;
    return node;
}

static void link_inode(inode_t *node, flink_t *file) {
    assertvf( (file->f_dir->d_mount == node->mnode),
            "Cannot link file *%x to inode *%x: different mnodes\n",
            (ptr_t)file, (ptr_t)node);

    assertv( null == file->f_inode, "Cannot relink\n");

    file->f_inode = node;
    ++ node->nlink;
}

/* Just creates the file and inserts it into directory
 * If dir is null, file won't be inserted to any directory
 */
static inline err_t make_file_link(
        dnode_t *dir, const char *name, flink_t **file) {
    assert( dir || file, EINVAL, "make_file_link()");

    flink_t *f = tmalloc(flink_t);
    assert (f, ENOMEM, "make_file_link(): ENOMEM");

    f->f_dir = dir;
    f->f_name = strdup(name);

    if (dir)
        list_insert(dir->d_files, f);

    if (file) *file = f;
    return 0;
}


static err_t new_directory(dnode_t *parent, const char *name, dnode_t **dir) {
    assert(dir || parent, EINVAL, "new_directory(**null)");

    dnode_t *newd = tmalloc(dnode_t);
    assert(newd, ENOMEM, "new_directory(): ENOMEM");


    flink_t *fnewd;
    make_file_link(parent, name, &fnewd);
    if (fnewd == null) {
        kfree(newd);
        logd("new_directory():1: ENOMEM");
        return ENOMEM;
    }

    fnewd->type = IT_DIR;
    fnewd->type_spec = newd;

    newd->d_file = fnewd;
    newd->d_parent = parent;
    if (parent)
        newd->d_mount = parent->d_mount;
    list_init(newd->d_files, null);

    if (dir) *dir = newd;
    return 0;
}

err_t vfs_mkdir(const char *path, mode_t mode) {
    logdf("mkdir('%s')\n", path);

    char *file_delim = strrchr(path, FS_DELIM);
    char *fname = file_delim + 1;

    assertq( file_delim , EINVAL);

    flink_t *fpdir = vfs_file_by_npath(path, fname - path);
    assertq( fpdir , ENOENT);

    dnode_t *newdir;
    new_directory(get_dnode(fpdir), fname, &newdir);

    return 0;
}

err_t mkdev(const char *path, mode_t mode, dev_t dev) {
    assertq( S_ISBLK(mode) || S_ISCHR(mode), EINVAL);
    assertq( dev_is_valid(dev), ENODEV);

    char *file_delim = strrchr(path, FS_DELIM);
    char *fname = file_delim + 1;
    assert( file_delim, EINVAL, "mkdev(): invalid path");

    flink_t *fpardir = vfs_file_by_npath(path, file_delim - path);
    assertq( fpardir, ENOENT);

    /*
    flink_t *file = tmalloc(flink_t);
    file->f_inode = new_inode(get_dnode(fpardir)->d_mount, mode);
    assert( file->f_inode, ENOMEM, "mkdev(): no new inode");

    file->type = IT_CHRDEV;
    file->f_name = strdup(fname);
    file->f_dir = get_dnode(fpardir);
    */

    return ETODO;
}

err_t vfs_mknod(const char *path, mode_t mode, dev_t dev) {
    logdf("vfs_mknod('%s', 0%o, %x)\n", path, mode, dev);

    switch (mode & S_IFMT) {
    case S_IFREG:
        return 0;
    case S_IFDIR:
        return vfs_mkdir(path, mode);
    case S_IFCHR: case S_IFBLK:
        return mkdev(path, mode, dev);
    default:
        logf("Mode %x is invalid or unimplemented\n", mode);
        return EINVAL;
    }
}


size_t vfs_writei(inode_t *node, void *buf, off_t offset, size_t length) {
    mnode_t *sb = node->mnode;
    assertf(node->ops->write, 0, "No write() operation for inode *%x\n", (ptr_t)node);

    size_t bwritten = node->ops->write(node, buf, offset, length);
    //logdf("vfs_writei(*%x, *%x, %x, %x) = %x\n", node, buf, offset, length, bwritten);
    return bwritten;
}

size_t vfs_readi(inode_t *node, void *buf, off_t offset, size_t length) {
    mnode_t *sb = node->mnode;
    assertf(node->ops->read, 0, "No read() operation for inode *%x\n", (ptr_t)node);

    size_t bread = node->ops->read(node, buf, offset, length);
    //logdf("vfs_readi(*%x, *%x, %x, %x) = %x\n", node, buf, offset, length, bread);
    return bread;
}



struct kshell_command;

static void print_info(const flink_t *f, const char *name) {
    size_t flen = 0;
    if (f->f_inode)
        flen = f->f_inode->length;

    printf("  %c *%x\t*%x\t*%x\t%d\t%s\n", f->type,
            f, (ptr_t)f->f_inode, (ptr_t)f->type_spec, flen,
            (name ? : f->f_name));
}

void print_ls(const char *arg){
    flink_t *file = vfs_file_by_path(arg);
    assertvf( file , "File '%s' not found\n", arg);

    switch (file->type) {
    case IT_DIR: {
        dnode_t *dir = get_dnode(file);
        print_info(file, ".");

        flink_t *f;
        list_foreach(f, dir->d_files)
            print_info(f, null);
       } break;
    case IT_FILE: {
        print_info(file, null);
        } break;
    default:
        printf("Unknown file type 0x%x\n", file->type);
    }
}

void print_mount(void) {
    struct mountpoints_list_t *mntlist = mountpoints_list;
    for (; mntlist; mntlist = mntlist->next) {
        const mnode_t *mnode = mntlist->mnode;
        printf("%s (sb=*%x)\t on %s\ttype %s (fs=*%x)\n",
                mnode->name, (ptr_t)mnode,
                mnode->at->d_file->f_name,
                mnode->fs->name, (ptr_t)mnode->fs);
    }
}



#include <std/stdio.h>

static void file_to_stdout(flink_t *flink, const char *fpath) {
    /*** low-level file writing  example ***/
    assertvf(flink, "Cannot find link for path '%s'\n", fpath);

    inode_t *fnode = flink->f_inode;
    assertvf(fnode, "No inode for link *%x\n", (ptr_t)fnode);

    const int bufsz = 256;
    char buf[bufsz];

    off_t pos = 0;

    print("-------\n");
    while (1) {
        size_t read_bytes = vfs_readi(fnode, buf, pos, bufsz);
        if (read_bytes == 0) break;
        if (read_bytes > bufsz) {
            print("\n[read_bytes > bufsz]\n");
            read_bytes = bufsz;
        }

        int i;
        for (i = 0; i < read_bytes; ++i)
            cprint(buf[i]);

        pos += read_bytes;
    }
    printf("\n----- Done, bytes read: %d\n", pos);
}

static void file_from_stdin(flink_t *flink, dnode_t *fdir, const char *fname) {
    /*** low-level file writing ***/
    const int bufsz = 256;
    char buf[bufsz];

    off_t pos = 0;

    inode_t *fnode;

    if (null == flink) {
        /* create a new one */
        make_file_link(get_dnode(fdir), fname, &flink);
        assertv(flink, "Cannot create filelink\n");
        logf("Filelink *%x created\n", (ptr_t)flink);
        flink->type = IT_FILE;

        mnode_t *sb = get_dnode(fdir)->d_mount;
        fnode = new_inode(sb, S_IFREG);
        assertv(fnode, "Cannot create inode\n");
        logf("Inode at *%x\n", (ptr_t)fnode);

        link_inode(fnode, flink);
    } else {
        logf("[existing filelink *%x will be rewritten]\n", flink);
        fnode = flink->f_inode;
    }

    print("----- Type, ctrl-d for exit:\n");
    while (1) {
        int ret = getline(buf, bufsz);

        size_t len = strlen(buf);
        size_t written = vfs_writei(fnode, buf, pos, len);
        if (written < len)
            printf("[only %d bytes written from %d]\n", written, len);

        pos += written;
        if (ret == 4) // ctrl-d
            break;
    }
    printf("----- Done, bytes written: %d\n", pos);
}

#define skip_spaces(s) do while (*s && (*s == ' ')) ++s; while(0)

void vfs_shell(const char *arg) {
    char *arg_end = strchr(arg, ' ');
    assertv(arg_end, "Usage:\twrite|read </full/path/to/file>\n"
            "\tmknod <type=-|d|c> </full/path/to> [<dev-maj> <dev-min>]\n"
            "\tmkdir </full/path/to>\n");

    /* get file path */
    bool file_write = !strncmp("write", arg, arg_end - arg);

    if (file_write || !strncmp("read", arg, arg_end - arg))
    {
        skip_spaces(arg_end);
        char *fpath = arg_end;

        /* get file name */
        char *fname = strrchr(fpath, FS_DELIM);
        assertv(fname, "Specify full path to file\n");

        flink_t *fdir = vfs_file_by_npath(fpath, fname - fpath);
        assertvf(fdir, "Cannot find directory for '%s'\n", fpath);
        ++fname; // to exclude '/' at the beginning

        flink_t *flink = vfs_file_by_path(fpath);

        if (file_write) file_from_stdin(flink, fdir, fname);
        else file_to_stdout(flink, fpath);
    }
    else if (!strncmp("mkdir", arg, arg_end - arg)) {
        skip_spaces(arg_end);
        vfs_mkdir(arg_end, S_IFDIR);
    }
    else if (!strncmp("mknod", arg, arg_end - arg)) {
        skip_spaces(arg_end);

        /* read type */
        assertv(arg_end[1] == ' ', "Type must be a single letter");
        char type = arg_end[0];
        ++arg_end;
        skip_spaces(arg_end);

        /** read path **/
        char *cur = arg_end;
        while (*cur && (*cur != ' ')) ++cur;
        char namebuf[cur - arg_end + 1];
        strncpy(namebuf, arg_end, cur - arg_end);
        namebuf[cur - arg_end] = 0;

        int major = 0, minor = 0;
        if ((type == IT_CHRDEV) || (type == IT_BLKDEV)) {
            while (*cur && (*cur != ' ')) ++cur;
            skip_spaces(cur);
            cur = sscan_int(cur, &major, 10);
            skip_spaces(cur);
            sscan_int(cur, &minor, 10);
        }
        err_t ret =
            vfs_mknod(namebuf, filetype_to_mode(type), makedev(major, minor));
        if (ret)
            printf("Error %d\n", ret);
    }
    else {
        print("?\n");
    }
}

#include <fs/ramfs.h>

void vfs_setup(void) {
    err_t retval = 0;

    retval = fs_register(get_ramfs());
    if (retval) logf("error: 0x%x\n", retval);

    retval = vfs_mount("rootfs", "/", "ramfs");
    if (retval) logf("error: 0x%x\n", retval);

    retval = vfs_mkdir("/etc", 0755);
    if (retval) logf("error: 0x%x\n", retval);

    retval = vfs_mkdir("/dev", 0755);
    if (retval) logf("error: 0x%x\n", retval);

    retval = vfs_mount("devfs", "/dev", "ramfs");
    if (retval) logf("error: 0x%x\n", retval);
}

#define __DEBUG

#include <vfs.h>
#include <std/string.h>
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

    DLINKED_LIST_INIT(null, null)   /* const */
};

flink_t *root_file = &dummy_root_file;

static err_t new_directory(dnode_t *parent, const char *name, dnode_t **dir);

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

flink_t* vfs_file_by_npath(const char *path, const size_t n) {
    return_if_eq (path, null);

    /* prepare pathname lookup */
    return_if (path[0] != FS_DELIM , null,
            "TODO: vfs_flink_by_path(): relative path resolution\n");

    if ( (path[1] == 0) || (n < 2))
        return root_file;

    dnode_t *current = get_dnode(root_file);
    char *start = path, *end = path;  // position of subdir name

    while (1) {
        start = end + 1;

        end = strchr(start, FS_DELIM);
        if (null == end)
            // FS_DELIM not found, path remains a stripped file name:
            return vfs_file_by_nname(current, start, n - (start - path));

        if ((start - path) >= (off_t)n)
            end = path + n;

        flink_t *subdir = vfs_file_by_nname(current, start, end - start);
        assertf (subdir, null, "No such a directory: %s\n", start);

        current = get_dnode(subdir);
        assertf (current, null, "Cannot get dnode for %s\n", start);
    }
}

flink_t* vfs_file_by_path(const char *path) {
    return vfs_file_by_npath(path, MAX_INT);
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
    assertq (mnode, ENOMEM);

    /* initialize fs-independent fields */
    mnode->n_deps = 0;
    mnode->fs = fs;

    /* initialize mountpoint */
    dnode_t *mntdir;
    new_directory(dir->d_parent, null, &mntdir);
    mntdir->d_file->f_name = dir->d_file->f_name;

    /* replace file link in dir->d_parent with new one */
    if (dir->d_parent) {
        list_release(dir->d_parent->d_files, dir->d_file);
        list_append(dir->d_parent->d_files, mntdir->d_file);
    }
    else // root directory
        root_file = mntdir->d_file;

    mnode->at = mntdir;

    /* insert mnode to mountlist */
    new_mountlist_node->mnode = mnode;
    new_mountlist_node->shadow_dir = dir;
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
    list_init(newd->d_files, null);

    if (dir) *dir = newd;
    return 0;
}

err_t vfs_mkdir(const char *path, uint mode) {
    logdf("mkdir('%s')\n", path);

    char *file_delim = strrchr(path, FS_DELIM);
    char *fname = file_delim + 1;

    if (file_delim == null)
        return EINVAL;

    flink_t *fpdir = vfs_file_by_npath(path, fname - path);
    if (null == fpdir)
        return ENOENT;

    dnode_t *newdir;
    new_directory(get_dnode(fpdir), fname, &newdir);

    return 0;
}


struct kshell_command;

void print_ls(const char *arg){
    flink_t *file = vfs_file_by_path(arg);
    assertvf( file , "File '%s' not found\n", arg);

    printf("  flink_t at *%x\n", file);

    switch (file->type) {
    case IT_DIR: {
        dnode_t *dir = get_dnode(file);
        printf("  dnode_t at *%x\n", dir);

        print("Files:\n");
        flink_t *f;
        list_foreach(f, dir->d_files)
            printf("    *%x\t%s\n", f, f->f_name);

        } break;
    default:
        printf("Unknown file type 0x%x\n", file->type);
    }
}

void print_mount(void) {
    struct mountpoints_list_t *mntlist = mountpoints_list;
    for (; mntlist; mntlist = mntlist->next) {
        const mnode_t *mnode = mntlist->mnode;
        k_printf("%s (sb=*%x)\t on %s\ttype %s (fs=*%x)\n",
                mnode->name, (ptr_t)mnode,
                mnode->at->d_file->f_name,
                mnode->fs->name, (ptr_t)mnode->fs);
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

    /*retval = vfs_mount("devfs", "/dev", "ramfs");
    if (retval) logf("error: 0x%x\n", retval);
    */
}

#include <vfs.h>
#include <std/string.h>
#include <log.h>

vfs_pool_t default_pool;
#define current_vfs_pool() (&default_pool)

extern dnode_t root_directory;

/*
 *      Root directory structures
 */
flink_t root_file = {
    .f_name = "/",                  /* const: the only file with / in name */
    .f_inode = null,                /* to be set on mounting rootfs */
    .f_dir = null,                  /* const: root file does not belong to any directory */
    .type = IT_DIR,                 /* const */
    .type_spec = &root_directory,   /* const */

    DLINKED_LIST_INIT(null, null)   /* const */
};

dnode_t root_directory = {
    .d_file = &root_file,           /* const */
    .d_mount = null,
    .d_files = null,
    .d_parent = null,               /* const */

    DLINKED_LIST_INIT(null, null)   /* const */
};


#define return_err_if(assertion, msg, errcode) \
    if (assertion) { k_printf((msg)); return errcode; }

#define return_err_if_not(assertion, msg, errcode) \
    if (! (assertion)) { print(msg); return errcode; }

#define return_if_equal(x1, x2) if ((x1) == (x2)) return (x2);

/*
 *    File paths and search
 */
flink_t* vfs_file_by_name(const dnode_t *dir, const char *fname) {
    flink_t *file;
    for (file = dir->d_files; file; file = list_next(file)) {
        if (!strcmp(fname, file->f_name))
            return file;
    }
    return null;
}

flink_t* vfs_file_by_nname(const dnode_t *dir, const void *fname, size_t n) {
    flink_t *file;
    for (file = dir->d_files; file; file = list_next(file)) {
        if (('\0' == file->f_name[n])
                && !memcmp(fname, file->f_name, n))
            return file;
    }
    return null;
}

flink_t* vfs_file_by_npath(const char *path, const size_t n) {
    if (path == null) return null;

    /* prepare pathname lookup */
    if (path[0] != FS_DELIM) {
        log("TODO: vfs_flink_by_path(): relative path resolution\n");
        return null;
    }

    if ((path[1] == 0) || (n < 2))
        return &root_file;

    dnode_t *current = &root_directory;
    const char *start = path, *end = path;  // position of subdir name

    while (1) {
        start = end + 1;

        end = strchr(start, FS_DELIM);
        if (null == end)
            // FS_DELIM not found, path remains a stripped file name:
            return vfs_file_by_nname(current, start, n - (start - path));

        if ((start - path) >= (off_t)n)
            end = path + n;

        flink_t *subdir = vfs_file_by_nname(current, start, end - start);
        if (! vfs_is_directory(subdir))
            return null;

        current = (dnode_t *) subdir->type_spec;
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
    if (null == fs) return EINVAL;

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
    struct mountpoints_list_t *new_mountlist_node =
            (struct mountpoints_list_t *) kmalloc(sizeof(struct mountpoints_list_t));
    if (null == new_mountlist_node)
        return ENOMEM;

    /* create superblock and initialize fs-dependent part */
    mnode_t *mnode = fs->new_superblock(source, null);
    if (null == mnode)
        return ENOMEM;

    /* initialize fs-independent fields */
    mnode->n_deps = 0;
    mnode->at = dir;
    mnode->fs = fs;

    // if (

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
    if (IT_DIR != target_flink->type)
        return ENOTDIR; /* target must be directory */

    dnode_t *dir = (dnode_t *)(target_flink->type_spec);
    reterr = vfs_is_dir_mountable(dir);
    if (reterr)
        return reterr;

    /* source must be mountable */
    reterr = check_device_mountability(source, fs);
    if (reterr)
        return reterr;

    return mount(source, dir, fs, null);
}

/* Just creates the file and inserts it into directory */
static inline err_t new_file(
        dnode_t *dir, const char *name, flink_t **file) {
    flink_t *f = (flink_t *) kmalloc( sizeof(flink_t) );
    return_err_if_not(f, "new_file(): ENOMEM", ENOMEM);

    f->f_dir = dir;
    f->f_name = strcpy(null, name);

    list_insert(dir->d_files, f);

    if (file) *file = f;
    return 0;
}


static err_t new_directory(dnode_t *parent, const char *name, dnode_t **dir) {
    dnode_t *newd = (dnode_t *) kmalloc( sizeof(dnode_t) );
    return_err_if_not(newd, "new_directory(): ENOMEM", ENOMEM);

    newd->d_parent = parent;

    flink_t *fnewd;
    return_err_if(
        new_file(parent, name, &fnewd),
        "new_directory():1: ENOMEM", ENOMEM);

    fnewd->type_spec = newd;
    fnewd->type = IT_DIR;

    newd->d_file = fnewd;

    if (dir) *dir = newd;
    return 0;
}

err_t vfs_mkdir(const char *path, uint mode) {
    logf("vfs_mkdir('%s')\n", path);

    const char *file_delim = strrchr(path, FS_DELIM);
    const char *fname = file_delim + 1;

    if (file_delim == null)
        return EINVAL;

    flink_t *fpdir = vfs_file_by_npath(path, fname - path);
    if (null == fpdir)
        return ENOENT;
    if (! vfs_is_directory(fpdir))
        return ENOTDIR;

    dnode_t *parent_dir = (dnode_t *) fpdir->type_spec;

    /* initialize a new dnode */
    return new_directory(parent_dir, fname, null);
}


struct kshell_command;

void print_ls(const char *arg){
    flink_t *file = vfs_file_by_path(arg);
    if (!file) {
        print("File not found\n");
        return;
    }

    printf("  flink_t at *%x\n", file);

    switch (file->type) {
    case IT_DIR: {
        dnode_t *dir = (dnode_t *) file->type_spec;
        printf("  dnode_t at *%x\n", arg, dir);

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
        k_printf("%s\t on %s\ttype %s\n",
                mnode->name, mnode->at->d_file->f_name, mnode->fs->name);
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

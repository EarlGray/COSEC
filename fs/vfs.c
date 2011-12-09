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
    .f_name = "/",                  /* const */
    .f_inode = null,
    .f_dir = null,                  /* const */
    .type = IT_DIR,                 /* const */
    .type_spec = &root_directory,   /* const */

    DLINKED_LIST_INIT(null, null)   /* const */
};

dnode_t root_directory = {
    .d_files = null,
    .d_subdirs = null,
    .d_parent = null,               /* const */

    DLINKED_LIST_INIT(null, null)   /* const */
};


#define return_err_if_not(assertion, msg, errcode) \
    if (! (assertion)) { k_printf((msg)); return errcode; }

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
        log("TODO: vfs_flink_by_path(): relative path resolution");
        return null;
    }

    if (path[1] == 0)
        return &root_file;

    dnode_t *current = &root_directory;
    char *start = path, *end = path;  // position of subdir name

    while (1) {
        start = end + 1;

        end = strchr(start, FS_DELIM);
        if (null == end)
            // FS_DELIM not found, path remains a stripped file name:
            return vfs_file_by_nname(current, start, n - (start - path));

        if ((start - path) >= n)
            end = path + n;

        flink_t *subdir = vfs_file_by_nname(start, end - start);
        if (!vfs_flink_type_is(subdir, IT_DIR))
            return null;

        current = (dnode_t *) subdir->type_spec;
    }
}

flink_t* vfs_file_by_path(const char *path) {
    return vfs_file_by_npath(path, MAX_UINT);
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
    struct filesystems_list_t *entry = kmalloc(sizeof(struct filesystems_list_t));
    list_link_next(entry, filesystems_list);
    entry->fs = fs;
    filesystems_list = entry;
    return 0;
}

filesystem_t * fs_by_name(const char *name) {
    struct filesystems_list_t *fslist;
    for (fslist = filesystems_list;
         fslist;
         fslist = list_next(fslist))
    {
        filesystem_t *fs = fslist->fs;

        if (!strcmp(fs->name, name))
            return fs;
    }
    return null;
};

#define mnode_parent(mnode) \
    ((mnode)->at->mnode)

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

    mnode_t *mnode = (mnode_t *) kmalloc(sizeof(mnode_t));
    if (null == mnode)
        return ENOMEM;

    mnode->deps_count = 0;
    mnode->at = dir;
    mnode->name = fs->get_name_for_device(source);

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
    reterr = check_device_mountability(source, fstype);
    if (reterr)
        return reterr;

    return mount(source, dir, fs, null);
}

int vfs_mkdir(const char *path, uint mode) {
    logf("TODO: vfs_mkdir('%s')\n", path);
    return 0;
}

struct kshell_command;

void print_ls(void){
    //
}

void print_mount(void) {
    struct mountpoints_list_t *mntlist = mountpoints_list;
    while (mntlist) {
        mnode_t *mnode = mntlist->mnode;
        k_printf("%s\t%s\n", mnode->name, mnode->fs->name);
        mntlist = mntlist->next;
    }
}

#include <fs/ramfs.h>

void vfs_setup(void) {
    fs_register(&ramfs);

    err_t retval = 0;
    retval = vfs_mount("rootfs", "/", "ramfs");
    if (retval) logf("error: 0x%x\n", retval);

    vfs_mkdir("/etc", 0755);
    vfs_mkdir("/dev", 0755);

    retval = vfs_mount("devfs", "/dev", "ramfs");
    if (retval) logf("error: 0x%x\n", retval);
}

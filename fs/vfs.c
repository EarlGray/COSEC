#include <vfs.h>
#include <std/string.h>
#include <log.h>

vfs_pool_t default_pool;
#define current_vfs_pool() (&default_pool) 

dnode_t root_directory = {
    .d_files = null,
    .d_subdirs = null,

    DLINKED_LIST_INIT(null, null)
};

#define return_err_if_not(assertion, msg, errcode) \
    if (! (assertion)) { k_printf((msg)); return errcode; }

#define return_if_equal(x1, x2) if ((x1) == (x2)) return (x2);

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

    flink_t *current = (flink_t *)&root_directory;

    /* part of 'path' containing current dirnode name */
    uint start_sym, end_sym;

    /* prepare pathname lookup */
    if (path[0] == FS_DELIM) {
        end_sym = 1;
    } else {
        log("TODO: vfs_flink_by_path(): relative path resolution");
        return null;
    }

    /* walk through 'path' */
    while (path[end_sym] && (end_sym < n)) {
        start_sym = end_sym + 1;

        do ++end_sym;
        while (path[end_sym] && (path[end_sym] != FS_DELIM) && (end_sym < n));

        if (end_sym == start_sym) 
            continue;

        current = vfs_file_by_nname(current, 
            path + start_sym, end_sym - start_sym);
        if (!current) 
            return null;
    }

    return current;
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
    struct filesystems_list_t *fslist = filesystems_list;
    while (fslist) {
        filesystem_t *fs = fslist->fs;

        if (!strcmp(fs->name, name)) 
            return fs;

        fslist = list_next(fslist);
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

err_t vfs_mount(const char *source, const char *target, const char *fstype) {
    logf("TODO: mount('%s', '%s', '%s')\n", source, target, fstype);
    return 0;
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

    vfs_mount("rootfs", "/", "ramfs");

    vfs_mkdir("/etc", 0755);
    vfs_mkdir("/dev", 0755);

    vfs_mount("devfs", "/dev", "ramfs");
}

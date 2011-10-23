#include <vfs.h>
#include <std/string.h>

vfs_pool_t default_pool;
#define current_vfs_pool() (&default_pool) 

dnode_t root_directory = {
    .d_mnode = null,
    .d_parent = null,
    .d_subdirs = null,
    .d_files = null,
    .d_next = null,
    .d_prev = null,
};

#define return_err_if_not(assertion, msg, errcode) \
    if (! (assertion)) { k_printf((msg)); return errcode; }

#define return_if_equal(x1, x2) if ((x1) == (x2)) return (x2);

flink_t* vfs_file_by_name(const dnode_t *dir, const char *fname) {
    flink_t *file = dir->d_files;
    for (; file; file = file->f_next) {
        if (!strcmp(fname, file->f_name))
            return file;
    }
    return null;
}

flink_t* vfs_file_by_nname(const dnode_t *dir, const void *fname, size_t n) {
    flink_t *file = dir->d_files;
    for (; file; file = file->f_next) {
        if (('\0' == file->f_name[n]) && !memcmp(fname, file->f_name, n))
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
        k_printf("TODO: vfs_flink_by_path(): relative path resolution");
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
    struct filesystems_list_t *next;
};

struct filesystems_list_t *filesystems_list = null;

err_t fs_register(filesystem_t *fs) {
    struct filesystems_list_t *entry = kmalloc(sizeof(struct filesystems_list_t));
    entry->next = filesystems_list;
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

        fslist = fslist->next;
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
    k_printf("mount('%s', '%s', '%s')\n", source, target, fstype);

    flink_t *atf = vfs_file_by_path(target);
    return_err_if_not(atf, "Node not found\n", ENODEV);
    return_err_if_not(atf->f_inode->type == IT_DIR, "Is not a directory", ENOTDIR);

    dnode_t *at = (dnode_t *) atf;

    filesystem_t *fs = fs_by_name(fstype);
    return_err_if_not(fs, "Unknown filesystem\n", ENOENT);

    const char *devname = fs->get_name_for_device(source);
    return_err_if_not(devname, "fstype is invalid for source", EINVAL);

    mnode_t *mnode = kmalloc(sizeof(mnode_t));
    return_err_if_not(mnode, null, ENOMEM);

    mnode->at = at;
    mnode->fs = fs;
    mnode->name = devname;

    mnode->deps_count = 0;

    /** parent mnode **/
    mnode_t *parent = at->d_mnode;
    if (parent != null) {
        ++parent->deps_count;
    }

    if (fs->populate_tree)
        fs->populate_tree(mnode);

    struct mountpoints_list_t *mountpoint = 
        kmalloc(sizeof(struct mountpoints_list_t));
    mountpoint->mnode = mnode;
    mountpoint->next = mountpoints_list;
    mountpoints_list = mountpoint;
    
    return 0;
}

int vfs_mkdir(const char *path, uint mode) {
    const char *name = strrchr(path, FS_DELIM);
    return_err_if_not(name, "TODO: mkdir for relative path\n", EINVAL);

    if (!name[1]) {     /* path is like "/dev/" */
        name = strnrchr(path, FS_DELIM, name - path);
        return_err_if_not(name, "TODO: mkdir for relative path\n", EINVAL);
    }
    ++name;

    flink_t *at = vfs_file_by_npath(path, name - path);
    return_err_if_not(at, "ENOENT", ENOENT);
    return_err_if_not(at->f_inode->type == IT_DIR, "Not a directory", ENOTDIR);

    dnode_t *dir = (dnode_t *)at;
    return_err_if_not(null == vfs_file_by_name(dir, name), 
            "Already exists", EEXIST);

    mnode_t *mnode = at->f_inode->mnode;

    flink_t *link = kmalloc(sizeof(flink_t));
    return_err_if_not(link, "No memory for file link", ENOMEM);
    link->f_name = strcpy(null, name);

    dnode_t *dnode = kmalloc(sizeof(dnode_t));
    return_err_if_not(dnode, "No memory for dnode", ENOMEM);

    return_err_if_not(mnode->fs->new_inode, "No new_inode() for fs", EINVAL);
    inode_t *inode = mnode->fs->new_inode(mnode);
    return_err_if_not(inode, "Can't allocate inode", ENOMEM);

    link->f_inode = inode;
    inode->mnode = mnode;
    inode->nlink = 1;
    inode->type = IT_DIR;
    inode->data = null;
    
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

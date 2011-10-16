#include <vfs.h>
#include <std/string.h>

vfs_pool_t default_pool;
#define current_vfs_pool() (&default_pool) 

dnode_t root_directory = {
    .mnode = null,
    .inode = null,
    .parent = null,
};

dnode_t* vfs_dnode_by_path(const char *path) {
    if (path == null) 
        return null;

    dnode_t *current;

    /* part of 'path' containing current dirnode name */
    uint start_sym, end_sym;

    if (path[0] == FS_DELIM) {
        if (path[1] == '\0') 
            return &root_directory;

        current = &root_directory;
        start_sym = end_sym = 1;
    } else {
        current = current_vfs_pool()->cwd;
        start_sym = 0;
        end_sym = 1;
        while (path[end_sym] && (FS_DELIM != path[end_sym])) ++end_sym;
    }

    return current;
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

#define return_err_if_not(assertion, msg, errcode) \
    if (! (assertion)) { k_printf((msg)); return errcode; }

err_t vfs_mount(const char *source, const char *target, const char *fstype) {
    k_printf("mount('%s', '%s', '%s')\n", source, target, fstype);

    dnode_t *at = vfs_dnode_by_path(target);
    return_err_if_not(at, 
            "Dirnode not found\n", ENODEV);

    filesystem_t *fs = fs_by_name(fstype);
    return_err_if_not(fs, 
            "Unknown filesystem\n", ENOENT);

    const char *devname = fs->get_name_for_device(source);
    return_err_if_not(devname, 
            "fstype is invalid for source", EINVAL);

    mnode_t *mnode = kmalloc(sizeof(mnode_t));
    return_err_if_not(mnode, null, ENOMEM);

    mnode->at = at;
    mnode->fs = fs;
    mnode->name = devname;

    mnode->deps_count = 0;

    /** parent mnode **/
    mnode_t *parent = at->mnode;
    if (parent != null) {
        ++parent->deps_count;
    }

    if (fs->populate_tree)
        fs->populate_tree(mnode);
    
    return 0;
}


int vfs_mkdir(const char *path, uint mode) {
    k_printf("TODO: mkdir('%s', %o)\n", path, mode);
    return 0;
}

#include <fs/ramfs.h>

void vfs_setup(void) {
    fs_register(&ramfs);

    vfs_mount("rootfs", "/", "ramfs");

    vfs_mkdir("/etc", 0755);
    vfs_mkdir("/dev", 0755);

    vfs_mount("devfs", "/dev", "ramfs");
}

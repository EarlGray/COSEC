#include <fs/fs.h>

struct filesystem *rootfs;

struct inode {
    
};

struct filesystem;

struct file {
    const char *name;
    struct filesystem *fs;
};

struct dentry {
    const char *name;
    struct filesystem *fs;
};

struct filesystem {
    const char *name;
    
};


void fs_setup(void) {
    err_t err;

    err = fs_load_initfs(rootfs);
    if (err) 
        panic("Initfs can't be loaded");

    
}


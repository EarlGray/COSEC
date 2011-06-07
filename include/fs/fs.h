#ifndef __FS_H__
#define __FS_H__

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

extern struct filesystem rootfs;

#endif //__FS_H__

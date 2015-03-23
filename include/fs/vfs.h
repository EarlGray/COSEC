#ifndef __VFS_H__
#define __VFS_H__

#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/dirent.h>
#include <sys/types.h>

#include <fs/devices.h>

#define FS_SEP  '/'

typedef uint16_t uid_t, gid_t;
typedef size_t inode_t;

typedef struct superblock   mountnode;
typedef struct inode        inode;
typedef struct filesystem_driver        fsdriver;
typedef struct filesystem_operations    fs_ops;


struct superblock {
    dev_t       sb_dev;
    fsdriver   *sb_fs;

    size_t      sb_blksz;
    struct {
        bool dirty :1 ;
        bool ro :1 ;
    } sb_flags;

    inode_t     sb_root_ino;      /* index of the root inode */
    void       *sb_data;          /* superblock-specific info */

    const char *sb_mntpath;       /* path relative to the parent mountpoint */
    uint32_t    sb_mntpath_hash;  /* hash of this relmntpath */
    mountnode  *sb_brother;       /* the next superblock in the list of childs of parent */
    mountnode  *sb_parent;
    mountnode  *sb_children;      /* list of this superblock child blocks */
};


#define N_DIRECT_BLOCKS  12
#define MAX_SHORT_SYMLINK_SIZE   60

struct inode {
    index_t i_no;               /* inode index */
    mode_t  i_mode;             /* inode type + unix permissions */
    count_t i_nlinks;
    off_t   i_size;             /* data size if any */
    void   *i_data;             /* fs- and type-specific info */

    union {
        struct {
            off_t block_coout;                     // how many blocks its data takes
            off_t directblock[ N_DIRECT_BLOCKS ];  // numbers of first DIRECT_BLOCKS blocks
            off_t indir1st_block;
            off_t indir2nd_block;
            off_t indir3rd_block;
        } reg;
        //struct { } dir;
        struct {
            majdev_t maj;
            mindev_t min;
        } dev;
        struct {
            char short_symlink[ MAX_SHORT_SYMLINK_SIZE ];
            const char *long_symlink;
        } symlink;
        //struct { } socket;
        //struct { } namedpipe;
    } as;
};


struct filesystem_driver {
    const char  *name;
    uint        fs_id;
    fs_ops      *ops;

    struct {
        fsdriver *prev, *next;
    } lst;
};



struct filesystem_operations {
    /**
     * \brief  probes the superblock on sb->sb_dev,
     * @param mountnode     the superblock to initialize
     */
    int (*read_superblock)(mountnode *sb);

    /**
     * \brief  creates a directory at local `path`
     * @param ino   if not NULL, it is set to inode index of the new directory;
     * @param path  fs-local path to the directory;
     * @param mode  POSIX mode (S_IFDIR will be set if needed)
     */
    int (*make_directory)(mountnode *sb, inode_t *ino, const char *path, mode_t mode);

    /**
     * \brief  creates a REG/CHR/BLK/FIFO/SOCK inode
     * @param ino       if not NULL, it is set to inode index of the new inode;
     * @param mode      POSIX mode (REG/CHR/BLK/FIFO/SOCK, will be set to REG if 0;
     * @param info      mode-dependent info;
     */
    int (*make_inode)(mountnode *sb, inode_t *ino, mode_t mode, void *info);

    /**
     * \brief  frees the inode
     * @param ino       the inode index
     */
    int (*free_inode)(mountnode *sb, inode_t ino);

    /**
     * \brief  copies generic inode with index `ino` into buffer `idata`
     * @
     */
    int (*inode_data)(mountnode *sb, inode_t ino, struct inode *idata);

    /**
     * \brief  reads/writes data from an inode blocks
     * @param pos       position (in bytes) to read from/write to
     * @param buf       a buffer to copy data to/from
     * @param buflen    the buffer length
     * @param written   if not NULL, returns number of actually copied bytes;
     */
    int (*read_inode)(mountnode *sb, inode_t ino, off_t pos,
                      char *buf, size_t buflen, size_t *written);

    int (*write_inode)(mountnode *sb, inode_t ino, off_t pos,
                       const char *buf, size_t buflen, size_t *written);

    /**
     * \brief  iterates through directory and fills `dir`.
     * @param ino   the directory inode index;
     * @param iter  an iterator pointer; `*iter` must be set to NULL before the first call;
     *              `*iter` will be set to NULL after the last directory entry;
     * @param dir   a buffer to fill;
     */
    int (*get_direntry)(mountnode *sb, inode_t ino, void **iter, struct dirent *dir);

    /**
     * \brief  searches for inode index at the fs-local `path`
     * @param ino       if not NULL, it is set to the found inode index;
     * @param path      start of name; must be fs-local;
     * @param pathlen   sets the end of path if there's no null bytes;
     */
    int (*lookup_inode)(mountnode *sb, inode_t *ino, const char *path, size_t pathlen);

    /**
     * \brief  creates a hardlink to inode with `path` (up to `pathlen` bytes)
     * @param ino       the inode to be linked;
     * @param path      fs-local path for the new (hard) link;
     * @param pathlen   limits `path` length up to `pathlen` bytes;
     */
    int (*link_inode)(mountnode *sb, inode_t ino, inode_t dirino, const char *name, size_t namelen);

    /**
     * \brief  deletes a hardlink with path `path`, possibly deletes the inode
     * @param path      the hardlink's path;
     * @param pathlen   limits `path` to `pathlen` bytes;
     */
    int (*unlink_inode)(mountnode *sb, const char *path, size_t pathlen);
};


typedef struct mount_opts_t  mount_opts_t;

struct mount_opts_t {
    uint fs_id;
    bool readonly:1;
};

extern struct inode theInvalidInode;

void vfs_register_filesystem(fsdriver *fs);
fsdriver * vfs_filesystem_by_id(uint fs_id);

int vfs_mountnode_by_path(const char *path, mountnode **mntnode, const char **relpath);
int vfs_path_dirname_len(const char *path, size_t pathlen);

int vfs_lookup(const char *path, mountnode **mntnode, inode_t *ino);
int vfs_mount(dev_t source, const char *target, const mount_opts_t *opts);
int vfs_stat(const char *path, struct stat *stat);
int vfs_mkdir(const char *path, mode_t mode);
int vfs_mknod(const char *path, mode_t mode, dev_t dev);
int vfs_hardlink(const char *path, const char *newpath);
int vfs_unlink(const char *path);
int vfs_rename(const char *oldpath, const char *newpath);

int vfs_inode_read(mountnode *sb, inode_t ino, off_t pos,
                   char *buf, size_t buflen, size_t *written);

int vfs_inode_write(mountnode *sb, inode_t ino, off_t pos,
                    const char *buf, size_t buflen, size_t *written);

void print_ls(const char *path);
void print_mount(void);
void vfs_setup(void);

#endif // __VFS_H__

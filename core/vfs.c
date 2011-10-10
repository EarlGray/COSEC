///#include <vfs.h>

/**
  *     Internal declarations
 */

struct fs_driver_struct;
struct dir_node;
struct mount_node;
struct index_node;

typedef  struct fs_driver_struct  fs_driver;
typedef  struct mount_node          mnode;
typedef  struct index_node          inode;
typedef  struct dir_node            dnode;

struct index_node {
    count_t usage;
};

struct dir_node {
    mnode *d_mount;
    inode *d_inode;

    dnode *d_parent;
};

struct mount_node {
    const char *name;
    const char *at;

    dnode *at_dir;
};

struct vfs_pool_struct {
    
};

int vfs_open(const char *fname, int flags) {

}

void vfs_setup(void) {
    vfs_mount("ramfs", "/", "ramfs")

    vfs_mkdir("/etc", 0755);
    vfs_mkdir("/dev", 0755);

    vfs_mount("devfs", "/dev", "ramfs");
}

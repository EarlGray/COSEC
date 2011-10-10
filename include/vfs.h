#ifndef __VIRTUAL_FILE_SYSTEM__
#define __VIRTUAL_FILE_SYSTEM__

/** File descriptors **/
typedef uint file_t;

/** Struct encapsulating all file resources relating to e.g. process **/
struct vfs_pool_struct;
typedef  struct vfs_pool_struct  vfs_pool;


/*** **************************************************************************
  *     Superblock operations 
 ***/

/** mount flags **/
#define MS_DEFAULT  0

struct mount_options_struct {
    const char *fs_str;     /* name of file system type, e.g. "ramfs" */
    uint flags;
};
typedef  struct mount_options_struct  mount_options;

/** Mount: file 'what' at 'at' as 'fs' **/
err_t vfs_mount(const char *what, const char *at, const char *fs);

/** Mount with options **/
err_t vfs_opt_mount(const char *what, const char *at, mount_options *mntopts);


/*** **************************************************************************
  *     File operations
 ***/

/** Try to open file by name **/
err_t vfs_open(const char *fname, int flags);

ssize_t vfs_read(file_t fd, void *buf, size_t count);
ssize_t vfs_write(file_t fd, void *buf, size_t count);

err_t vfs_close(file_t fd);



/*** **************************************************************************
  *     Directory operations
 ***/

int vfs_mkdir(const char *path, uint mode);

void vfs_setup(void);

#endif // __VIRTUAL_FILE_SYSTEM__

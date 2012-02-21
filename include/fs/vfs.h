#ifndef __VFS_H__
#define __VFS_H__

struct mount_opts_strct;
typedef  struct mount_opts_strct  mount_opts;

err_t vfs_mount(const char *source, const char *target, const mount_opts *opts);
err_t vfs_mkdir(const char *path, mode_t mode);

void print_ls(const char *path);
void print_mount(void);
void vfs_shell(const char *);

void vfs_setup(void);

#endif // __VFS_H__

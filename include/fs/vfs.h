#ifndef __VFS_H__
#define __VFS_H__

#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>

void print_ls(const char *path);
void print_mount(void);
void vfs_setup(void);

#endif // __VFS_H__

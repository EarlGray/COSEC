#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/errno.h>
#include <unistd.h>

#define ETODO ENOTSUP 

#define FUSE_API_VERSION 26
#define FUSE_USE_VERSION 26

#include <fuse.h>

const char *hello_path = "/hello";
const char *hello_file = "Hello World!\n";

static void *fatfs_fuse_init(struct fuse_conn_info *conn) {
    FILE *f = fopen("/tmp/fatfuse.log", "a");
    if (f) {
        fprintf(f, "\ninit()\n"); fflush(f);
    }
    return f;
}

static void fatfs_fuse_destroy(void *data) {
    struct fuse_context *fc = fuse_get_context();
    FILE *f = fc->private_data;
    fprintf(f, "destroy()\n\n");
    fclose(f);
}

static int fatfs_fuse_statfs(const char *fs, struct statvfs *stat) {
    FILE *f = fuse_get_context()->private_data;
    fprintf(f, "statfs(%s, %p)\n", fs, stat);
    fflush(f);

    stat->f_bsize = 512;
    stat->f_blocks = 1;
    stat->f_bfree = 0;
    stat->f_bavail = 0;
    stat->f_files = 1;
    stat->f_ffree = 0;
    stat->f_favail = 0;
    stat->f_namemax = 256;

    return 0;
} 

static int fatfs_fuse_getattr(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));
    if (!strcmp(path, "/")) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else if (!strcmp(path, hello_path)) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = strlen(hello_file);
    } else return -ENOENT;
    return 0;
}

static int fatfs_fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                              off_t offset, struct fuse_file_info *fi)
{
    (void)offset; (void)fi;

    if (strcmp(path, "/"))
        return -ENOENT;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    filler(buf, hello_path + 1, NULL, 0);
    return 0;
}

struct fuse_operations fs_ops = {
    .getattr =      fatfs_fuse_getattr,
    .readdir =      fatfs_fuse_readdir,
    .readlink =     NULL, //fat_fuse_readlink,
    .getdir =       NULL, //fat_fuse_getdir,
    .mknod =        NULL, //fat_fuse_mknod, 
    .mkdir =        NULL, //fat_fuse_mkdir,
    .unlink =       NULL, //fat_fuse_unlink,
    .rmdir =        NULL, //fat_fuse_rmdir,
    .symlink =      NULL, //fat_fuse_symlink,
    .rename =       NULL, //fat_fuse_rename,
    .link =         NULL, //fat_fuse_link,
    .chmod =        NULL, //fat_fuse_chmod,
    .chown =        NULL, //fat_fuse_chown,
    .truncate =     NULL, //fat_fuse_truncate,
    .utime =        NULL, //fat_fuse_utime,
    .access =       NULL, //fat_fuse_access,
    .open =         NULL, //fat_fuse_open,
    .read =         NULL, //fat_fuse_read,
    .write =        NULL, //fat_fuse_write,
    .statfs =       fatfs_fuse_statfs,
    .flush =        NULL, //fat_fuse_flush,
    .release =      NULL, //fat_fuse_release,
    .fsync =        NULL, //fat_fuse_fsync,
    .setxattr =     NULL, //fat_fuse_setxattr,
    .getxattr =     NULL, //fat_fuse_getxattr,
    .listxattr =    NULL, //fat_fuse_listxattr,
    .removexattr =  NULL, //fat_fuse_removexattr,
    
    .init =         fatfs_fuse_init,
    .destroy =      fatfs_fuse_destroy,
};

int main(int argc, char **argv) {
    return fuse_main(argc, argv, &fs_ops, NULL);
}

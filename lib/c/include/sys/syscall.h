#ifndef __COSEC_LIBC_COSEC_SYS__
#define __COSEC_LIBC_COSEC_SYS__

#ifdef LINUX
# define SYSCALL_NO_TLS  1
# include <linux/sysnum.h>
# include <linux/syscall_i386.h>
#endif  // LINUX

#ifdef COSEC_KERN
# include <cosec/sysnum.h>
#endif  // COSEC_KERN

#ifdef COSEC
# include <cosec/sysnum.h>

static inline
int syscall(int num, intptr_t arg1, intptr_t arg2, intptr_t arg3) {
    int ret;
    asm volatile ("int $0x80  \n"
        :"=a"(ret)
        :"a"(num), "c"(arg1), "d"(arg2), "b"(arg3)
        :"memory");
    return ret;
}

#define __syscall0(num)             syscall((num), 0, 0, 0)
#define __syscall1(num, a)          syscall((num), (a), 0, 0)
#define __syscall2(num, a, b)       syscall((num), (a), (b), 0)
#define __syscall3(num, a, b, c)    syscall((num), (a), (b), (c))

#endif  // COSEC


#include <stdint.h>
#include <stdarg.h>
#include <signal.h>

#include <sys/stat.h>


/* logging */
int lprintf(const char *fmt, ...);
int vlprintf(const char *fmt, va_list ap);

/*
 *  sys_*() handlers
 */
struct mount_info {
    dev_t       source;
    const char *target;
    const char *fstype;
    uint flags;
    void *opts;
};

typedef  struct mount_info  mount_info_t;

int sys_mount(mount_info_t *);
int sys_umount(const char *target);

int sys_fstat(int fd, struct stat *stat);

struct cosec_dirent {
    index_t d_ino;
    off_t   d_off;
    size_t  d_reclen;
    char    d_name[];
};

int sys_mkdir(const char *pathname, mode_t mode);
int sys_lsdir(const char *pathname, struct cosec_dirent *dirs, count_t count);
int sys_rmdir(const char *pathname);

int sys_chdir(const char *path);
char *sys_pwd(char *buf);

int sys_rename(const char *oldpath, const char *newpath);
int sys_link(const char *oldpath, const char *newpath);
int sys_symlink(const char *oldpath, const char *newpath);
int sys_unlink(const char *pathname);

int sys_open(const char *pathname, int flags);
int sys_read(int fd, void *buf, size_t count);
int sys_write(int fd, const void *buf, size_t count);
int sys_close(int fd);

off_t sys_lseek(int fd, off_t offset, int whence);
int sys_ftruncate(int fd, off_t length);

pid_t sys_getpid(void);
int sys_setpgid(pid_t pid, pid_t pgid);
pid_t sys_setsid(void);

pid_t sys_waitpid(pid_t pid, int *wstatus, int flags);
int sys_kill(pid_t pid, int sig);

intptr_t sys_brk(void *addr);
pid_t sys_fork(void);
int sys_execve(const char *pathname, char *const argv[], char *const envp[]);
void sys_exit(int status);
sighandler_t sys_signal(int signum, sighandler_t handler);

#endif  // __COSEC_LIBC_COSEC_SYS__

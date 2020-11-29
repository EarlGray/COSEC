#ifndef __COSEC_STDC_SYSCALL
#define __COSEC_STDC_SYSCALL

#define N_SYSCALLS      0x100

#define SYS_exit        0x01
#define SYS_fork        0x02
#define SYS_read        0x03
#define SYS_write       0x04
#define SYS_open        0x05
#define SYS_close       0x06
#define SYS_waitpid     0x07
#define SYS_link        0x09
#define SYS_unlink      0x0a
#define SYS_execve      0x0b
#define SYS_chdir       0x0c
#define SYS_time        0x0d
#define SYS_mknod       0x0e
#define SYS_chmod       0x0f
#define SYS_stat        0x12
#define SYS_lseek       0x13
#define SYS_getpid      0x14

#define SYS_mount       0x15
#define SYS_umount      0x16

#define SYS_kill        0x25
#define SYS_rename      0x26
#define SYS_mkdir       0x27
#define SYS_rmdir       0x28

#define SYS_dup         0x29
#define SYS_pipe        0x2a

#define SYS_brk         0x2d

#define SYS_pread       0x31
#define SYS_pwrite      0x33
#define SYS_trunc       0x35

#define SYS_setpgid     0x39
#define SYS_setsid      0x42
#define SYS_sigaction   0x43

#define SYS_fstat       0x6c

#define SYS_print       0xff

#ifndef NOT_CC

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

#endif
#endif

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
#define SYS_ioctl       0x36

#define SYS_setpgid     0x39
#define SYS_setsid      0x42
#define SYS_sigaction   0x43

#define SYS_fstat       0x6c

#define SYS_print       0xff

#endif

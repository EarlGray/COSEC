#ifndef __COSEC_STDC_SYSCALL
#define __COSEC_STDC_SYSCALL

#define N_SYSCALLS      0x100

#define SYS_EXIT        0x01
#define SYS_FORK        0x02
#define SYS_READ        0x03
#define SYS_WRITE       0x04
#define SYS_OPEN        0x05
#define SYS_CLOSE       0x06
#define SYS_WAITPID     0x07
#define SYS_LINK        0x09
#define SYS_UNLINK      0x0a
#define SYS_EXECVE      0x0b
#define SYS_CHDIR       0x0c
#define SYS_TIME        0x0d
#define SYS_MKNOD       0x0e
#define SYS_CHMOD       0x0f
#define SYS_STAT        0x12
#define SYS_LSEEK       0x13
#define SYS_GETPID      0x14

#define SYS_MOUNT       0x15
#define SYS_UMOUNT      0x16

#define SYS_KILL        0x25
#define SYS_RENAME      0x26
#define SYS_MKDIR       0x27
#define SYS_RMDIR       0x28

#define SYS_DUP         0x29
#define SYS_PIPE        0x2a

#define SYS_BRK         0x2d

#define SYS_PREAD       0x31
#define SYS_PWRITE      0x33
#define SYS_TRUNC       0x35

#define SYS_PRINT       0xff

#ifndef NOT_CC

static inline
int syscall(int num, int arg1, int arg2, int arg3) {
    int ret;
    asm(
        "movl %3, %%ebx         \n"
        "movl %2, %%edx         \n"
        "movl %1, %%ecx         \n"
        "movl %0, %%eax         \n"
        "int $0x80              \n"
        :"=a"(ret):"m"(num), "m"(arg1), "m"(arg2), "m"(arg3)
    );
    return ret;
}

#endif
#endif

#ifndef __COSEC_SYS_WAIT_H__
#define __COSEC_SYS_WAIT_H__

#include <stdint.h>

/* these are part of Linux ABI too */

#define WNOHANG     1
#define WUNTRACED   2
#define WSTOPPED    2
#define WEXITED     4
#define WCONTINUED  8

/* TODO: make it Linux-compatible */
#define WIFEXITED(wstatus)      ((wstatus) & WEXITED)
#define WEXITSTATUS(wstatus)    ((wstatus) & 0xff)
#define WIFSIGNALED(wstatus)    (WTERMSIG(wstatus) != 0)
#define WTERMSIG(wstatus)       (int)((wstatus >> 8) & 0xff)
#define WIFSTOPPED(wstatus)     ((wstatus) & WSTOPPED)
#define WSTOPSIG(wstatus)       (int)((wstatus >> 8) & 0xff)
#define WIFCONTINUED(wstatus)   ((wstatus) & WCONTINUED)

pid_t wait(int *wstatus);

pid_t waitpid(pid_t pid, int *wstatus, int options);

#endif  // __COSEC_SYS_WAIT_H__

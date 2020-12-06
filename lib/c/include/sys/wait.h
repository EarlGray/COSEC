#ifndef __COSEC_SYS_WAIT_H__
#define __COSEC_SYS_WAIT_H__

#include <stdint.h>

/* these are part of Linux ABI too */

#define WNOHANG     1
#define WUNTRACED   2
#define WSTOPPED    2
#define WEXITED     4
#define WCONTINUED  8

#define __WCOREFLAG 0x80

#define WTERMSIG(wstatus)       ((wstatus) & 0x7f)
#define WIFEXITED(wstatus)      (WTERMSIG(wstatus) == 0)
#define WEXITSTATUS(wstatus)    (((wstatus) & 0xff00) >> 8)
#define WIFSIGNALED(wstatus)    \
  (((char) (((wstatus) & 0x7f) + 1) >> 1) > 0)
#define WIFSTOPPED(wstatus)     (((wstatus) & 0xff == 0x7f))
#define WSTOPSIG(wstatus)       (WEXITSTATUS(wstatus))
#define WIFCONTINUED(wstatus)   ((wstatus) == 0xffff)

pid_t wait(int *wstatus);

pid_t waitpid(pid_t pid, int *wstatus, int options);

#endif  // __COSEC_SYS_WAIT_H__

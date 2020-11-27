#ifndef __COSEC_PROCESS_H__
#define __COSEC_PROCESS_H__

#include <stdint.h>
#include <sys/types.h>

#include <cosec/syscall.h>

#include "mem/paging.h"
#include "fs/vfs.h"
#include "tasks.h"

/* should be 65536 */
#define NPROC_MAX       10

/* temporary value */
#define N_PROCESS_FDS   20

#define PID_INIT    1
#define PID_COSECD  2


typedef struct process  process;
typedef struct filedesc filedescr;

typedef struct filedesc {
    mountnode  *fd_sb;
    inode_t     fd_ino;

    uint        fd_flags;
    off_t       fd_pos;
} filedescr_t;

typedef struct process {
    task_struct ps_task;        /* context-switching info, keep it first */

    void *      ps_userstack;   /* vaddr of bottom of the user stack */
    void *      ps_heap_end;

    pid_t   ps_pid;
    pid_t   ps_ppid;

    mindev_t    ps_tty;         /* controlling tty */
    mode_t      ps_umask;       /* umask */
    char *      ps_cwd;         /* current directory */

    filedescr   ps_fds[N_PROCESS_FDS];
} process_t;

pid_t current_pid(void);
process * current_proc(void);
process * proc_by_pid(pid_t pid);

int process_grow_stack(process_t *, void *faultaddr);

int alloc_fd_for_pid(pid_t pid);
filedescr * get_filedescr_for_pid(pid_t pid, int fd);

void run_init(void);
void proc_setup(void);

#endif //__COSEC_PROCESS_H__

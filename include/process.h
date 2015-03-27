#ifndef __COSEC_PROCESS_H__
#define __COSEC_PROCESS_H__

#include <fs/vfs.h>
#include <tasks.h>

/* should be 65536 */
#define NPROC_MAX       10

/* temporary value */
#define N_PROCESS_FDS   20

typedef uint32_t        pid_t;
typedef struct process  process;
typedef struct filedesc filedescr;

struct filedesc {
    mountnode  *fd_sb;
    inode_t     fd_ino;

    off_t       fd_pos;
};

struct process {
    pid_t   ps_pid;
    pid_t   ps_ppid;

    void *      ps_kernstack;
    task_struct ps_task;        /* context-switching info */
    mindev_t    ps_tty;         /* controlling tty */
    filedescr   ps_fds[N_PROCESS_FDS];
};

process * proc_by_pid(pid_t pid);
process * current_proc(void);

void proc_setup(void);

#endif //__COSEC_PROCESS_H__

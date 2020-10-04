#ifndef __COSEC_LIBC_SIGNAL__
#define __COSEC_LIBC_SIGNAL__

/* ISO C99 signals.  */
#define SIGINT      2   /* Interactive attention signal.  */
#define SIGILL      4   /* Illegal instruction.  */
#define SIGABRT     6   /* Abnormal termination.  */
#define SIGFPE      8   /* Erroneous arithmetic operation.  */
#define SIGSEGV     11  /* Invalid access to storage.  */
#define SIGTERM     15  /* Termination request.  */

/* Historical signals specified by POSIX. */
#define SIGHUP      1   /* Hangup.  */
#define SIGQUIT     3   /* Quit.  */
#define SIGTRAP     5   /* Trace/breakpoint trap.  */
#define SIGKILL     9   /* Killed.  */
#define SIGBUS      10  /* Bus error.  */
#define SIGSYS      12  /* Bad system call.  */
#define SIGPIPE     13  /* Broken pipe.  */
#define SIGALRM     14  /* Alarm clock.  */

/* New(er) POSIX signals (1003.1-2008, 1003.1-2013).  */
#define SIGURG      16  /* Urgent data is available at a socket.  */
#define SIGSTOP     17  /* Stop, unblockable.  */
#define SIGTSTP     18  /* Keyboard stop.  */
#define SIGCONT     19  /* Continue.  */
#define SIGCHLD     20  /* Child terminated or stopped.  */
#define SIGTTIN     21  /* Background read from control terminal.  */
#define SIGTTOU     22  /* Background write to control terminal.  */
#define SIGPOLL     23  /* Pollable event occurred (System V).  */
#define SIGXCPU     24  /* CPU time limit exceeded.  */
#define SIGXFSZ     25  /* File size limit exceeded.  */
#define SIGVTALRM   26  /* Virtual timer expired.  */
#define SIGPROF     27  /* Profiling timer expired.  */
#define SIGUSR1     30  /* User-defined signal 1.  */
#define SIGUSR2     31  /* User-defined signal 2.  */

/* Nonstandard signals found in all modern POSIX systems
   (including both BSD and Linux).  */
#define SIGWINCH    28  /* Window size change (4.3 BSD, Sun).  */

/* Archaic names for compatibility.  */
#define SIGIO       SIGPOLL /* I/O now possible (4.2 BSD).  */
#define SIGIOT      SIGABRT /* IOT instruction, abort() on a PDP-11.  */
#define SIGCLD      SIGCHLD /* Old System V name */


#ifndef NOT_CC

#include <sys/types.h>

int raise(int sig);

int kill(pid_t pid, int sig);

#endif  // NOT_CC
#endif  // __COSEC_LIBC_SIGNAL__

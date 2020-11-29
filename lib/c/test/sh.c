#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>


#define COMMAND_SIZE    512
#define PROMPT_SIZE     128

#define MAX_TOKENS      (COMMAND_SIZE/2)

const char *default_ifs = " \t\r\f\n";
const char *default_prompt = "sh$ ";


/*
 *  Job handling
 */

#define MAX_JOBS    100

struct job {
    pid_t process_group;
    bool stopped;
};

struct shell {
    pid_t self_pid;
    pid_t foreground_group;

    const char *prompt;
    bool interactive;

    struct job * jobs[MAX_JOBS];
    ssize_t n_jobs;
};

void shell_set_foreground(struct shell *sh, pid_t pgid) {
    if (!sh->interactive)
       return;
    if (tcsetpgrp(STDIN_FILENO, pgid) < 0) {
        perror("tcsetpgrp");
    } else {
        sh->foreground_group = pgid;
    }
}

struct job *
shell_add_job(struct shell *sh, pid_t pid) {
    int i;
    for (i = 0; i < sh->n_jobs; ++i) {
        if (sh->jobs[i] == NULL) {
            break;
        }
    }
    if (i == MAX_JOBS) {
        fprintf(stderr, "jobs are exhausted\n");
        abort();
    }
    if (i == sh->n_jobs) {
        ++sh->n_jobs;
    }

    struct job *job = malloc(sizeof(struct job));
    job->stopped = false;
    job->process_group = pid;
    sh->jobs[i] = job;
    return job;
}

struct job *
shell_find_job(struct shell *sh, pid_t pid) {
    for (int i = 0; i < sh->n_jobs; ++i) {
        if (sh->jobs[i] && sh->jobs[i]->process_group == pid) {
            return sh->jobs[i];
        }
    }
    return NULL;
}

void shell_remove_job(struct shell *sh, pid_t pid) {
    for (int i = 0; i < sh->n_jobs; ++i) {
        if (sh->jobs[i] && sh->jobs[i]->process_group == pid) {
            free(sh->jobs[i]);
            sh->jobs[i] = NULL;
            return;
        }
    }
}

void shell_run_foreground(struct shell *sh, struct job *job) {
    pid_t pid = job->process_group;

    shell_set_foreground(sh, pid);

    if (job->stopped) {
        kill(pid, SIGCONT);
        job->stopped = false;
    }

    int wstatus;
    int wpid;
    while ((wpid = waitpid(-1, &wstatus, WUNTRACED))) {
        if (wpid == -1) { perror("waitpid^^"); }
        if (WIFSTOPPED(wstatus)) {
            fprintf(stderr, "%d stopped\n", wpid);
            struct job* job = shell_find_job(sh, wpid);
            if (job) {
                job->stopped = true;
            }
            if (wpid == pid) break;
        }
        if (WIFEXITED(wstatus)) {
            fprintf(stderr, "%d exited, status=%d\n", wpid, WEXITSTATUS(wstatus));
            shell_remove_job(sh, wpid);
            if (wpid == pid) break;
        }
        if (WIFSIGNALED(wstatus)) {
            fprintf(stderr, "%d terminated by signal %d, status=%d\n",
                    wpid, WTERMSIG(wstatus), WEXITSTATUS(wstatus));
            shell_remove_job(sh, wpid);
            if (wpid == pid) break;
        }
    }

    //fprintf(stderr, "the foreground group is %d", tcgetpgrp(STDIN_FILENO));
    shell_set_foreground(sh, sh->self_pid);
}

/*
 *  Builtin commands
 */

static int builtin_exit(struct shell *sh, char **tokens, size_t n_tokens) {
    // TODO: if unsaved jobs
    exit(0);
}

static int builtin_echo(struct shell *sh, char **tokens, size_t n_tokens) {
    ssize_t i;
    for (i = 1; i < n_tokens-1; ++i) {
        fprintf(stdout, "%s ", tokens[i]);
    }
    fprintf(stdout, "%s\n", tokens[i]);
    return 0;
}

static int builtin_jobs(struct shell *sh, char **tokens, size_t n_tokens) {
    for (size_t i = 0; i < sh->n_jobs; ++i) {
        struct job *job = sh->jobs[i];
        if (job) {
            fprintf(stdout, "%%%zd\t%s\tpid=%d\n",
                    i, job->stopped ? "stopped" : "running" , job->process_group);
        }
    }
    return 0;
}

static int builtin_bg(struct shell *sh, char **tokens, size_t n_tokens) {
    size_t i = 0;
    if (n_tokens == 2) {
        if (tokens[1][0] == '%') {
            i = atoi(tokens[1] + 1);
        }
        if (!sh->jobs[i]) {
            fprintf(stderr, "unknown job: %%%zd\n", i);
            return -1;
        }
    } else if (n_tokens == 1) {
        for (i = 0; i < sh->n_jobs; ++i) {
            if (sh->jobs[i])
                break;
        }
        if (!sh->jobs[i]) {
            fprintf(stderr, "no jobs\n");
            return -1;
        }
    } else {
        fprintf(stderr, "usage: `bg` or `bg %%<job number>`\n");
        return -1;
    }

    struct job *job = sh->jobs[i];
    if (job->stopped) {
        kill(job->process_group, SIGCONT);
        job->stopped = false;
    }
    return 0;
}

static int builtin_fg(struct shell *sh, char **tokens, size_t n_tokens) {
    size_t i = 0;
    if (n_tokens == 2) {
        if (tokens[1][0] == '%') {
            i = atoi(tokens[1] + 1);
        }
        if (!sh->jobs[i]) {
            fprintf(stderr, "unknown job: %%%zd\n", i);
            return -1;
        }
    } else if (n_tokens == 1) {
        for (i = 0; i < sh->n_jobs; ++i) {
            if (sh->jobs[i])
                break;
        }
        if (!sh->jobs[i]) {
            fprintf(stderr, "no jobs\n");
            return -1;
        }
    } else {
        fprintf(stderr, "usage: `fg` or `fg %%<job number>`\n");
        return -1;
    }

    pid_t pid = sh->jobs[i]->process_group;
    fprintf(stderr, "attached to pid=%d\n", pid);

    shell_run_foreground(sh, sh->jobs[i]);
    return 0;
}

struct builtin {
    const char *name;
    int (*func)(struct shell *sh, char **tokens, size_t n_tokens);
};

const struct builtin builtins[] = {
    { .name = "bg",     .func = builtin_bg   },
    { .name = "echo",   .func = builtin_echo },
    { .name = "exit",   .func = builtin_exit },
    { .name = "fg",     .func = builtin_fg   },
    { .name = "jobs",   .func = builtin_jobs },

    { .name = NULL,     .func = NULL }
};


/*
 *  Signal handlers
 */

void handle_sighup(int arg) {
    fprintf(stderr, "SIGHUP\n");
    // TODO: send SIGHUP to all jobs
    exit(0);
}

void handle_tstop(int arg) {
    fprintf(stderr, "SIGTSTP %d\n", arg);
}

void handle_child(int arg) {
    fprintf(stderr, "SIGCHLD %d\n", arg);
}

int main() {
    int ret;
    char cmdbuf[COMMAND_SIZE];

    ssize_t n_tokens;
    char *token_ctx = NULL;
    char *tok = NULL;
    char *s = cmdbuf;

    /* SIGTTOU is received on tcsetpgrp() and stops the process */
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTSTP, handle_tstop);

    signal(SIGQUIT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGHUP, handle_sighup);
    //signal(SIGCHLD, handle_child);

    setsid();

    struct shell sh;
    sh.interactive = isatty(STDIN_FILENO);
    sh.prompt = default_prompt;
    sh.self_pid = getpid();
    sh.n_jobs = 0;
    memset(sh.jobs, 0, MAX_JOBS * sizeof(struct jobs *));

    fprintf(stderr, "isatty = %d\n", sh.interactive);

    for (;;) {
        /* anything happened in background?
         * TODO: collect zombies immediately, somehow (select? timeouts? volatile queue?)
         */
        int wstatus;
        pid_t wpid;
        while (1) {
            wpid = waitpid((pid_t)-1, &wstatus, WNOHANG | WUNTRACED | WCONTINUED);
            if (wpid == 0) break;
            if (wpid == -1) {
                if (errno != ECHILD) perror("@@waitpid");
                break;
            }
            if (WIFEXITED(wstatus)) {
                fprintf(stderr, "Background job %d exited, status=%d\n", wpid, WEXITSTATUS(wstatus));
                shell_remove_job(&sh, wpid);
            } else if (WIFSIGNALED(wstatus)) {
                fprintf(stderr, "Background job %d killed, status=%d\n", wpid, WEXITSTATUS(wstatus));
                shell_remove_job(&sh, wpid);
            } else if (WIFSTOPPED(wstatus)) {
                fprintf(stderr, "Background job %d stopped\n", wpid);
                struct job *job = shell_find_job(&sh, wpid);
                if (job) { job->stopped = true; }
            } else if (WIFCONTINUED(wstatus)) {
                // fprintf(stderr, "Background job %d resumed\n", wpid);
                struct job *job = shell_find_job(&sh, wpid);
                if (job) { job->stopped = false; }
            }
        }

        /* print prompt and read the command */
        if (sh.interactive) {
            fprintf(stderr, "%s", sh.prompt);
        }
        if (!fgets(cmdbuf, COMMAND_SIZE, stdin)) {
            if (sh.interactive) fprintf(stdout, "\n");
            break;
        }


        /* tokenize the command */
        char *tokens[MAX_TOKENS];
        size_t toksize[MAX_TOKENS];
        n_tokens = 0;

        token_ctx = NULL;
        tok = NULL;
        s = cmdbuf;
        while ((tok = strtok_r(s, default_ifs, &token_ctx))) {
            tokens[n_tokens] = tok;
            toksize[n_tokens] = strlen(tok);
            //fprintf(stderr, "token[size=%zd]: %s\n", toksize[n_tokens], tokens[n_tokens]);

            ++n_tokens;
            s = NULL;
            if (n_tokens >= MAX_TOKENS-1) {
                fprintf(stderr, "Command has too many tokens\n");
                n_tokens = 0;
                break;
            }
        }

        if (n_tokens < 1) {
            continue;
        }

        /* builtin commands */
        const struct builtin *b;
        for (b = builtins; b->name; ++b) {
            if (strcmp(b->name, tokens[0]) == 0) {
                break;
            }
        }
        if (b->name) {
            b->func(&sh, tokens, n_tokens);
            continue;
        }

        tokens[n_tokens] = NULL;
        ++n_tokens;

        /* run a child */
        pid_t pid = fork();
        if (pid == 0) {
            /* child */
            pid = getpid();
            /* create a new job */
            if (setpgid(pid, 0)) {
                perror("setpgid");
            }

            /* jump into the child */
            execvp(tokens[0], tokens);

            /* should be unreachable */
            perror("execv"); 
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            /* parent */
            struct job *job = shell_add_job(&sh, pid);

            /* set foreground group to the child */
            shell_run_foreground(&sh, job);
        } else {
            perror("fork");
        }
    } 

    return EXIT_SUCCESS;
}

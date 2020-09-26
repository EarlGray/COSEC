#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/errno.h>

#define __DEBUG
#include <cosec/log.h>

#include <process.h>
#include <dev/tty.h>
#include <fs/vfs.h>
#include <mem/pmem.h>

#include <arch/mboot.h>


/*
 *  Global state
 */
pid_t theCurrentPID;
process_t * theProcessTable[NPROC_MAX] = { 0 };


/*
 *  PID management
 */

static pid_t
alloc_pid(void) {
    pid_t i;
    for (i = 1; i < NPROC_MAX; ++i) {
        if (NULL == theProcessTable[i]) {
            return i;
        }
    }
    return 0;
}

process_t * proc_by_pid(pid_t pid) {
    if (pid > NPROC_MAX) return 0;
    return theProcessTable[pid];
}

pid_t current_pid(void) {
    return theCurrentPID;
}

process_t * current_proc(void) {
    return theProcessTable[theCurrentPID];
}

int alloc_fd_for_pid(pid_t pid) {
    int i;
    process *p = proc_by_pid(pid);
    return_dbg_if(!p, EKERN, "%s: no process with pid %d\n", __func__, pid);

    for (i = 0; i < N_PROCESS_FDS; ++i)
        if (p->ps_fds[i].fd_ino < 1)
            return i;

    return -1;
}

filedescr * get_filedescr_for_pid(pid_t pid, int fd) {
    process *p = proc_by_pid(pid);
    return_dbg_if(p == NULL, NULL,
            "%s: no process with pid %d\n", __func__, pid);
    return_dbg_if(!((0 <= fd) && (fd < N_PROCESS_FDS)), NULL,
            "%s: fd=%d out of range\n", __func__, fd);

    return p->ps_fds + fd;
}


int sys_getpid() {
    return theCurrentPID;
}

/*
 *      Global scheduling and task dispatch
 */


/*
 *      Test init process
 *   temporary init test: use physical memory if applicable
 */
#include <linux/elf.h>

const char elf_magic[4] = { ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3 };

static const module_t *find_init_module(void) {
    int i;
    int initmodule = -1;
    count_t nmods = 0;
    module_t *minfo = NULL;
    mboot_modules_info(&nmods, &minfo);
    for (i = 0; i < (int)nmods; ++i) {
        if (0 == minfo[i].string)
            continue;
        if (!strcmp("init", (char *)minfo[i].string)) {
            return minfo + i;
        }
    }
    return NULL;
}

static bool elf_is_runnable(Elf32_Ehdr *elfhdr) {
    int ret;

    ret = strncmp((const char *)elfhdr->e_ident, elf_magic, 4);
    return_msg_if(ret, false,
            "%s: ELF magic is invalid", __func__);

    ret = (elfhdr->e_machine == EM_386);
    return_msg_if(!ret, false,
            "%s: ELF arch is not EM_386", __func__);

    ret = (elfhdr->e_ident[EI_CLASS] == ELFCLASS32);
    return_msg_if(!ret, false,
            "%s: ELF class is %d, not ELFCLASS32", __func__, (uint)elfhdr->e_type);

    return_msg_if(elfhdr->e_ident[EI_VERSION] != 1, false,
            "%s: ELF version is %d\n", __func__, elfhdr->e_version);
    return_msg_if(elfhdr->e_ident[EI_DATA] != ELFDATA2LSB, false,
            "%s: ELF cpu flags = 0x%x\n", __func__, elfhdr->e_flags);
    return_msg_if(elfhdr->e_type != ET_EXEC, false,
            "%s: ELF file is not executable(%d)\n", __func__, elfhdr->e_type);

    return true;
}

void run_init(void) {
    const module_t *initmod = NULL;
    size_t i;

    /* find module named `init` */
    initmod = find_init_module();
    returnv_msg_if(!initmod,
            "%s: module `init` not found", __func__);
    logmsgif("%s: ok, found module 'init' at *%x",
            __func__, initmod->mod_start);

    /* parse it as ELF file */
    char *elfmem = (char *)initmod->mod_start;
    Elf32_Ehdr *elfhdr = (Elf32_Ehdr*)elfmem;
    bool ok = elf_is_runnable(elfhdr);
    returnv_msg_if(!ok, "%s: parsing ELF failed", __func__);
    logmsgif("%s: ok, 'init' is a correct ELF binary", __func__);

    /* determine if its memory does not conflict */
    for (i = 0; i < elfhdr->e_shnum; ++i) {
        Elf32_Shdr *section = (Elf32_Shdr *)(elfmem + elfhdr->e_shoff + i * elfhdr->e_shentsize);
        if (!(section->sh_flags & SHF_ALLOC))
            continue;

        char *sect_start = (char *)section->sh_addr;
        char *sect_end = sect_start + section->sh_size;

        logmsgdf("%s: checking *%x-*%x\n", __func__, (uint)sect_start, (uint)sect_end);
        uint ret = pmem_check_avail(sect_start, sect_end);
        returnv_msg_if(ret,
                "%s: section[%d] cannot be allocated\n", __func__, i);
    }

    /* copy ELF sections there */
    for (i = 0; i < elfhdr->e_shnum; ++i) {
        Elf32_Shdr *section = (Elf32_Shdr *)(elfmem + elfhdr->e_shoff + i * elfhdr->e_shentsize);
        if (!(section->sh_flags & SHF_ALLOC))
            continue;

        char *sect_start = (char *)section->sh_addr;
        char *sect_end = sect_start + section->sh_size;

        logmsgdf("%s: init section[%d]: *%x-*%x, file offset 0x%x\n",
                __func__, i, (uint)sect_start, (uint)sect_end, section->sh_offset);
        pmem_reserve(sect_start, sect_end);
        memcpy(sect_start, elfmem + section->sh_offset, section->sh_size);
    }

    /* TODO: allocate stack and heap regions */

    logmsgif("%s: ready to rock!", __func__);

    /* TODO: start tasks */
#endif
}

/*
 *      Kernel thread: cosecd
 */
process_t theCosecThread;

extern char kern_stack;

void cosecd_setup(int pid) {
    const char *funcname = __FUNCTION__;

    theCosecThread.ps_pid = pid;
    theCosecThread.ps_kernstack = &kern_stack;
    theCosecThread.ps_tty = CONSOLE_TTY;
    theCosecThread.ps_pagedir = thePageDirectory;

    /* initialize input/output from /dev/tty0 */
    int ret = 0;
    mountnode *sb = NULL;
    inode_t ino = 0;

    filedescr_t * fds = theCosecThread.ps_fds;
    filedescr *infd =   fds + STDIN_FILENO;
    filedescr *outfd =  fds + STDOUT_FILENO;
    filedescr *errfd =  fds + STDERR_FILENO;
    logmsgdf("infd = *%x\n", (uint)infd);

    ret = vfs_lookup("/dev/tty0", &sb, &ino);
    returnv_err_if(ret, "%s: vfs_lookup('/dev/tty0'): %s", __func__, strerror(ret));
    logmsgdf("/dev/tty0 ino=%d\n", ino);

    infd->fd_sb  = outfd->fd_sb  = errfd->fd_sb  = sb;
    infd->fd_ino = outfd->fd_ino = errfd->fd_ino = ino;
    infd->fd_pos = outfd->fd_pos = errfd->fd_pos = -1;

    /* make it official */
    theProcessTable[pid] = &theCosecThread;
}


/*
 *      Global processes setup
 */

void proc_setup(void) {
    /* PID 0 is invalid */
    theProcessTable[0] = NULL;

    /* the kernel thread `cosecd` with pid=2, keep pid=1 for init */
    cosecd_setup(2);
    theCurrentPID = 2;
}


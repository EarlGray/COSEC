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
pid_t theCurrPID;
pid_t theAllocPID = 1;

process theInitProc;
process * theProcTable[NPROC_MAX] = { 0 };


/*
 *  PID management
 */

static pid_t
alloc_pid(void) {
    pid_t i;
    for (i = theAllocPID; i < NPROC_MAX; ++i)
        if (NULL == theProcTable[i]) {
            theAllocPID = i;
            return i;
        }

    for (i = 1; i < theAllocPID; ++i)
        if (NULL == theProcTable[i]) {
            theAllocPID = i;
            return i;
        }

    return 0;
}

process * proc_by_pid(pid_t pid) {
    if (pid > NPROC_MAX) return 0;
    return theProcTable[pid];
}

pid_t current_pid(void) {
    return theCurrPID;
}

process * current_proc(void) {
    return theProcTable[theCurrPID];
}

int alloc_fd_for_pid(pid_t pid) {
    const char *funcname = __FUNCTION__;
    int i;
    process *p = proc_by_pid(pid);
    return_dbg_if(!p, EKERN, "%s: no process with pid %d\n", funcname, pid);

    for (i = 0; i < N_PROCESS_FDS; ++i)
        if (p->ps_fds[i].fd_ino < 1)
            return i;

    return -1;
}

filedescr * get_filedescr_for_pid(pid_t pid, int fd) {
    const char *funcname = __FUNCTION__;
    process *p = proc_by_pid(pid);
    return_dbg_if(p == NULL, NULL,
            "%s: no process with pid %d\n", funcname, pid);
    return_dbg_if(!((0 <= fd) && (fd < N_PROCESS_FDS)), NULL,
            "%s: fd=%d out of range\n", funcname, fd);

    return p->ps_fds + fd;
}


int sys_getpid() {
    return theCurrPID;
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
    const char *funcname = __FUNCTION__;
    int ret;

    ret = strncmp((const char *)elfhdr->e_ident, elf_magic, 4);
    return_msg_if(ret, false,
            "%s: ELF magic is invalid", funcname);

    ret = (elfhdr->e_machine == EM_386);
    return_msg_if(!ret, false,
            "%s: ELF arch is not EM_386", funcname);

    ret = (elfhdr->e_ident[EI_CLASS] == ELFCLASS32);
    return_msg_if(!ret, false,
            "%s: ELF class is %d, not ELFCLASS32", funcname, (uint)elfhdr->e_type);

    return_msg_if(elfhdr->e_ident[EI_VERSION] != 1, false,
            "%s: ELF version is %d\n", funcname, elfhdr->e_version);
    return_msg_if(elfhdr->e_ident[EI_DATA] != ELFDATA2LSB, false,
            "%s: ELF cpu flags = 0x%x\n", funcname, elfhdr->e_flags);
    return_msg_if(elfhdr->e_type != ET_EXEC, false,
            "%s: ELF file is not executable(%d)\n", funcname, elfhdr->e_type);

    return true;
}

void run_init(void) {
    const char *funcname = __FUNCTION__;
    const module_t *initmod = NULL;
    size_t i;

    /* find module named `init` */
    initmod = find_init_module();
    returnv_msg_if(!initmod,
            "%s: module `init` not found", funcname);
    logmsgif("%s: ok, found module 'init' at *%x",
            funcname, initmod->mod_start);

    /* parse it as ELF file */
    char *elfmem = (char *)initmod->mod_start;
    Elf32_Ehdr *elfhdr = (Elf32_Ehdr*)elfmem;
    bool ok = elf_is_runnable(elfhdr);
    returnv_msg_if(!ok, "%s: parsing ELF failed", funcname);
    logmsgif("%s: ok, 'init' is a correct ELF binary", funcname);

    /* determine if its memory does not conflict */
    for (i = 0; i < elfhdr->e_shnum; ++i) {
        Elf32_Shdr *section = (Elf32_Shdr *)(elfmem + elfhdr->e_shoff + i * elfhdr->e_shentsize);
        if (!(section->sh_flags & SHF_ALLOC))
            continue;

        char *sect_start = (char *)section->sh_addr;
        char *sect_end = sect_start + section->sh_size;

        logmsgdf("%s: checking *%x-*%x\n", funcname, (uint)sect_start, (uint)sect_end);
        uint ret = pmem_check_avail(sect_start, sect_end);
        returnv_msg_if(ret,
                "%s: section[%d] cannot be allocated\n", funcname, i);
    }

    /* copy ELF sections there */
    for (i = 0; i < elfhdr->e_shnum; ++i) {
        Elf32_Shdr *section = (Elf32_Shdr *)(elfmem + elfhdr->e_shoff + i * elfhdr->e_shentsize);
        if (!(section->sh_flags & SHF_ALLOC))
            continue;

        char *sect_start = (char *)section->sh_addr;
        char *sect_end = sect_start + section->sh_size;

        logmsgdf("%s: init section[%d]: *%x-*%x, file offset 0x%x\n",
                funcname, i, (uint)sect_start, (uint)sect_end, section->sh_offset);
        pmem_reserve(sect_start, sect_end);
        memcpy(sect_start, elfmem + section->sh_offset, section->sh_size);
    }

    /* TODO: allocate stack and heap regions */

    logmsgif("%s: ready to rock!", funcname);

    /* TODO: start tasks */
}

/*
 *      Global processes setup
 */
extern char kern_stack;

void proc_setup(void) {
    const char *funcname = __FUNCTION__;
    /* invalid */
    theProcTable[0] = NULL;

    /* there is the init process at start up */
    theCurrPID = 1;
    theProcTable[theCurrPID] = &theInitProc;

    theInitProc.ps_pid = theCurrPID;
    theInitProc.ps_ppid = 0;
    theInitProc.ps_tty = CONSOLE_TTY;
    theInitProc.ps_kernstack = &kern_stack;

    /* temporary hack */
    /* init process should initialize its descriptors from userspace */
    int ret = 0;
    mountnode *sb = NULL;
    inode_t ino = 0;

    ret = vfs_lookup("/dev/tty0", &sb, &ino);
    returnv_err_if(ret, "%s: vfs_lookup('/dev/tty0'): %s", funcname, strerror(ret));
    logmsgdf("/dev/tty0 ino=%d\n", ino);

    filedescr *infd = theInitProc.ps_fds + STDIN_FILENO;
    filedescr *outfd = theInitProc.ps_fds + STDOUT_FILENO;
    filedescr *errfd = theInitProc.ps_fds + STDERR_FILENO;
    logmsgdf("infd = *%x\n", (uint)infd);

    infd->fd_sb  = outfd->fd_sb  = errfd->fd_sb  = sb;
    infd->fd_ino = outfd->fd_ino = errfd->fd_ino = ino;
    infd->fd_pos = outfd->fd_pos = errfd->fd_pos = -1;
}


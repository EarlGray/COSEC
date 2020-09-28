#include "kshell.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/errno.h>

#define __DEBUG
#include <cosec/log.h>

#include "arch/i386.h"
#include "arch/mboot.h"
#include "conf.h"
#include "mem/pmem.h"
#include "mem/paging.h"
#include "dev/tty.h"
#include "fs/vfs.h"
#include "tasks.h"

#include "process.h"


#define USER_STACK_TOP  (KERN_OFF - PAGE_SIZE)


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

static bool elf_is_runnable(const Elf32_Ehdr *elfhdr) {
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

process_t theInitProcess;

void run_init(void) {
    const module_t *initmod = NULL;
    size_t i;

    /* find module named `init` */
    initmod = find_init_module();
    returnv_msg_if(!initmod,
            "%s: module `init` not found", __func__);
    logmsgif("%s: ok, found module 'init' at *%x",
            __func__, initmod->mod_start);
    size_t elf_size = initmod->mod_end - initmod->mod_start;

    /* parse it as ELF file */
    uint8_t *elfmem = (uint8_t *)initmod->mod_start;
    const Elf32_Ehdr *elfhdr = (Elf32_Ehdr*)elfmem;

    bool ok = elf_is_runnable(elfhdr);
    returnv_msg_if(!ok, "%s: parsing ELF failed", __func__);
    logmsgif("%s: ok, 'init' is a correct ELF binary", __func__);

    void *entry = (void*)elfhdr->e_entry;
    logmsgif("%s:   entry = *%0.8x", __func__, entry);

    /* allocate a page directory */
    void *pagedir = pagedir_alloc();
    assertv(pagedir, "%s: failed to allocate pagedir", __func__);
    logmsgf("%s: pagedir = @%x\n", __func__, pagedir);

    /* read program headers */
    assertv((size_t)elfhdr->e_phoff < elf_size,
            "%s: e_phoff=0x%0.8x > elfsize=0x%0.8x", elfhdr->e_phoff, elf_size);
    Elf32_Phdr *program_headers = (Elf32_Phdr *)(elfmem + elfhdr->e_phoff);

    for (i = 0; i < elfhdr->e_phnum; ++i) {
        Elf32_Phdr hdr = program_headers[i];
        logmsgif("%s:   %d\t0x%0.8x[%x]\t0x%0.8x[%x]\t0x%x", __func__,
            hdr.p_type, hdr.p_offset, hdr.p_filesz, hdr.p_vaddr, hdr.p_memsz, hdr.p_align);

        if (hdr.p_type != PT_LOAD)
            continue;

        assertv(hdr.p_align == PAGE_SIZE,
                "%s: p_align=0x%x, PT_LOAD not on page boundary", __func__, hdr.p_align);
        assertv((hdr.p_vaddr & 0xFFF) == 0,
                "%s: p_vaddr=0x%x, not on page boundary", __func__, hdr.p_vaddr);

        for (uintptr_t off = 0; off < hdr.p_memsz; off += PAGE_SIZE) {
            uintptr_t vaddr = hdr.p_vaddr + off;

            uint32_t mask = PTE_WRITABLE; // TODO: check if
            void *paddr = pagedir_get_or_new(pagedir, (void*)vaddr, mask);
            assertv(paddr, "%s: failed to allocate page at *%x", __func__, vaddr);

            void *page = __va(paddr);

            size_t bytes_to_copy = 0;
            if (off < hdr.p_filesz) {
                bytes_to_copy = hdr.p_filesz - off;
                if (bytes_to_copy > PAGE_SIZE)
                    bytes_to_copy = PAGE_SIZE;
            }
            memset(page + bytes_to_copy, 0, PAGE_SIZE - bytes_to_copy);
            memcpy(page, elfmem + hdr.p_offset + off, bytes_to_copy);
        }
    }

    /* allocate stack and heap regions */
    void *kernstack = pmem_alloc(1);
    assertv(kernstack, "%s: failed to allocate kernstack", __func__);
    logmsgf("%s: kernstack @%x\n", __func__, kernstack);
    void *esp0 = __va(kernstack) + PAGE_SIZE - 0x20;

    void *userstack = (void *)(USER_STACK_TOP - PAGE_SIZE);
    void *stack = pagedir_get_or_new(pagedir, userstack, PTE_WRITABLE);
    logmsgf("%s: userstack @%x\n", __func__, stack);

    /* setting the new process */
    const pid_t pid = PID_INIT;
    process_t *proc = &theInitProcess;
    proc->ps_ppid = 0;
    proc->ps_pid = pid;
    proc->ps_cwd = "/";
    proc->ps_tty = CONSOLE_TTY;

    const segment_selector cs = { .as.word = SEL_USER_CS };
    const segment_selector ds = { .as.word = SEL_USER_DS };
    void *esp = userstack + PAGE_SIZE - 0x10;
    task_init(&proc->ps_task, entry,
            esp0, esp, cs, ds
    );
    proc->ps_task.tss.cr3 = (uintptr_t)pagedir;

    proc->ps_userstack = userstack;

#if 1
    /* run the process */
    theProcessTable[pid] = proc;
    logmsgif("%s: ready to rock!", __func__);
#else
    logmsgif("%s: TODO", __func__);
#endif
    return;

cleanup_pagedir:
    pagedir_free(pagedir);
    return;
}

/*
 *      Kernel thread: cosecd
 */
process_t theCosecThread;

extern char kern_stack;

void cosecd_setup(int pid) {
    /* initialize tss and memory */
    void *pagedir = __pa(thePageDirectory);

    theCosecThread.ps_pid = pid;
    theCosecThread.ps_tty = CONSOLE_TTY;

    task_struct *task = &theCosecThread.ps_task;

    task->kstack = &kern_stack;
    task->kstack_size = KERN_STACK_SIZE;
    task->entry = kshell_run;

    void *esp0 = task->kstack + task->kstack_size - 0x10;
    task_kthread_init(task, task->entry, esp0);
    task->tss.cr3 = (uintptr_t)pagedir;

    /* initialize input/output from /dev/tty0 */
    int ret = 0;
    mountnode *sb = NULL;
    inode_t ino = 0;

    filedescr_t * fds = theCosecThread.ps_fds;
    filedescr *infd =   fds + STDIN_FILENO;
    filedescr *outfd =  fds + STDOUT_FILENO;
    filedescr *errfd =  fds + STDERR_FILENO;
    logmsgdf("%s: infd = *%x\n", __func__, (uint)infd);

    ret = vfs_lookup("/dev/tty0", &sb, &ino);
    returnv_err_if(ret, "%s: vfs_lookup('/dev/tty0'): %s", __func__, strerror(ret));
    logmsgdf("%s: /dev/tty0 ino=%d\n", __func__, ino);

    infd->fd_sb  = outfd->fd_sb  = errfd->fd_sb  = sb;
    infd->fd_ino = outfd->fd_ino = errfd->fd_ino = ino;
    infd->fd_pos = outfd->fd_pos = errfd->fd_pos = -1;

    /* make it official */
    theProcessTable[pid] = &theCosecThread;
    logmsgdf("%s: ready\n", __func__);
}


/*
 *      Global processes setup
 */

void proc_setup(void) {
    /* PID 0 is invalid */
    theProcessTable[0] = NULL;

    /* the kernel thread `cosecd` with pid=2, keep pid=1 for init */
    cosecd_setup(PID_COSECD);
    logmsgdf("%s: ready\n", __func__);

    theCurrentPID = PID_COSECD;

    tasks_setup(&theCosecThread.ps_task);   // noreturn, must be last
    logmsgef("%s: unreachable", __func__);  // a load-bearing logmsgef
}

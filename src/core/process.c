#include "kshell.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
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


#define USER_STACK_TOP  (KERN_OFF - PAGE_BYTES)


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
    return current_proc()->ps_pid;
}

int sys_getpid() {
    return current_pid();
}

process_t * current_proc(void) {
    return (process_t *)task_current();
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


/*
 *  Process memory
 */

/* Physical address of the pagedir */
inline void *process_pagedir(process_t *proc) {
    return (void *)proc->ps_task.tss.cr3;
}

int process_grow_stack(process_t *proc, void *faultaddr) {
    // TODO: check max process stack
    // TODO: check any other mappings
    void *pagedir = process_pagedir(proc);

    while (proc->ps_userstack > faultaddr) {
        void *ustack = proc->ps_userstack - PAGE_BYTES;
        void *paddr = pagedir_get_or_new(pagedir, ustack, PTE_WRITABLE | PTE_USER);
        assert(paddr, -1, "%s: cannot allocate page for *%x\n", __func__, ustack);

        void *page = __va(paddr);
        memset(page, 0, PAGE_BYTES);

        proc->ps_userstack = ustack;
    }

    return 0;
}

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

        const char *name = (const char *)minfo[i].string;
        if (name[0] == '/') ++name;
        if (!strcmp("init", name)) {
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

static int process_attach_tty(process_t *proc, const char *ttyfile) {
    int ret = 0;
    mountnode *sb = NULL;
    inode_t ino = 0;

    filedescr_t * fds = proc->ps_fds;
    filedescr *infd =   fds + STDIN_FILENO;
    filedescr *outfd =  fds + STDOUT_FILENO;
    filedescr *errfd =  fds + STDERR_FILENO;
    logmsgdf("%s: infd = *%x\n", __func__, (uintptr_t)infd);

    ret = vfs_lookup(ttyfile, &sb, &ino);
    return_err_if(ret, ret, "%s: vfs_lookup('%s'): %s",
                   __func__, ttyfile, strerror(ret));
    logmsgdf("%s: %s ino=%d\n", __func__, ttyfile, ino);

    infd->fd_sb  = outfd->fd_sb  = errfd->fd_sb  = sb;
    infd->fd_ino = outfd->fd_ino = errfd->fd_ino = ino;
    infd->fd_pos = outfd->fd_pos = errfd->fd_pos = -1;

    struct stat st;
    ret = vfs_inode_stat(sb, ino, &st);
    return_err_if(ret, ret, "%s: vfs_stat: %s", __func__, strerror(ret));
    return_err_if(!S_ISCHR(st.st_mode), ENOTTY,
                  "%s: not a chardev: %s", __func__, ttyfile);

    mindev_t ttyno = gnu_dev_minor(st.st_rdev);
    logmsgdf("%s: %s has mindev %d\n", __func__, ttyfile, ttyno);

    return tty_set_foreground_procgroup(ttyno, proc->ps_pid);
}

static int process_segment_from_memory(Elf32_Phdr hdr, void *elfmem, void *pagedir) {
    assert(hdr.p_align == PAGE_BYTES, -EINVAL,
            "%s: p_align=0x%x, PT_LOAD not on page boundary", __func__, hdr.p_align);

    size_t off = 0;
    if (hdr.p_vaddr & 0xFFF) {
        uintptr_t vaddr = hdr.p_vaddr & 0xFFFFF000;
        size_t voff = hdr.p_vaddr & 0xFFF;
        size_t size = PAGE_BYTES - voff;

        uint32_t mask = PTE_WRITABLE | PTE_USER;
        void *paddr = pagedir_get_or_new(pagedir, (void*)vaddr, mask);
        assert(paddr, -ENOMEM, "%s: failed to allocate page at *%x", __func__, vaddr);

        void *page = __va(paddr);

        size_t bytes_to_copy = size;
        if (bytes_to_copy > hdr.p_filesz) {
            bytes_to_copy = hdr.p_filesz;
        }
        // TODO: set the preceding part to 0 to if it's a fresh page
        memcpy(page + voff, elfmem + hdr.p_offset + off, bytes_to_copy);
        memset(page + voff + bytes_to_copy, 0, size - bytes_to_copy);

        off += size;
    }

    for (; off < hdr.p_memsz; off += PAGE_BYTES) {
        uintptr_t vaddr = hdr.p_vaddr + off;

        uint32_t mask = PTE_WRITABLE | PTE_USER; // TODO: check if
        void *paddr = pagedir_get_or_new(pagedir, (void*)vaddr, mask);
        assert(paddr, -ENOMEM, "%s: failed to allocate page at *%x", __func__, vaddr);

        void *page = __va(paddr);

        size_t bytes_to_copy = 0;
        if (off < hdr.p_filesz) {
            bytes_to_copy = hdr.p_filesz - off;
            if (bytes_to_copy > PAGE_BYTES)
                bytes_to_copy = PAGE_BYTES;
        }
        memcpy(page, elfmem + hdr.p_offset + off, bytes_to_copy);
        memset(page + bytes_to_copy, 0, PAGE_BYTES - bytes_to_copy);
    }

    return 0;
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

    uintptr_t off;
    for (i = 0; i < elfhdr->e_phnum; ++i) {
        Elf32_Phdr hdr = program_headers[i];
        logmsgif("%s:   %d\t0x%0.8x[%x]\t0x%0.8x[%x]\t0x%x", __func__,
            hdr.p_type, hdr.p_offset, hdr.p_filesz, hdr.p_vaddr, hdr.p_memsz, hdr.p_align);

        if (hdr.p_type != PT_LOAD)
            continue;

        int ret = process_segment_from_memory(hdr, elfmem, pagedir);
        if (ret) {
            logmsgef("%s: segment load failed: %d", __func__, ret);
            goto cleanup_pagedir;
        }
    }

    /* allocate stack and heap regions */
    void *kernstack = pmem_alloc(1);
    assertv(kernstack, "%s: failed to allocate kernstack", __func__);
    logmsgf("%s: kernstack @%x\n", __func__, kernstack);
    void *esp0 = __va(kernstack) + PAGE_SIZE - 0x20;

    void *userstack = (void *)(USER_STACK_TOP - PAGE_SIZE);
    void *stack = pagedir_get_or_new(pagedir, userstack, PTE_WRITABLE | PTE_USER);
    logmsgf("%s: userstack @%x\n", __func__, stack);

    /* setting the new process */
    const pid_t pid = PID_INIT;
    process_t *proc = &theInitProcess;
    proc->ps_ppid = 0;
    proc->ps_pid = pid;
    proc->ps_cwd = "/";
    proc->ps_tty = CONSOLE_TTY;
    proc->ps_userstack = userstack;
    proc->ps_heap_end = (void *)pagealign_up(heap_end);

    const segment_selector cs = { .as.word = SEL_USER_CS };
    const segment_selector ds = { .as.word = SEL_USER_DS };
    void *esp = userstack + PAGE_BYTES - 0x10;

    task_init(&proc->ps_task, entry,
            esp0, esp, cs, ds
    );
    proc->ps_task.tss.cr3 = (uintptr_t)pagedir;

    /* file descriptors */
#if 1
    process_attach_tty(proc, "/dev/tty1");
    const char *msg = "\n\tCOSEC tty1\n\tPress F1 to go back to tty0\n\n";
    tty_write(1, msg, strlen(msg));
    tty_switch(1);
#else
    process_attach_tty(proc, "/dev/tty0");
#endif

    /* run the process */
    theProcessTable[pid] = proc;
    sched_add_task(&proc->ps_task);
    logmsgif("%s: ready to rock!\n", __func__);
    return;

cleanup_pagedir:
    pagedir_free(pagedir);
    return;
}

/*
 *      Kernel thread: cosecd
 */
process_t theCosecThread;

extern uint8_t kern_stack[KERN_STACK_SIZE];

void cosecd_setup(int pid) {
    /* initialize tss and memory */
    void *pagedir = __pa(thePageDirectory);

    theCosecThread.ps_pid = pid;
    theCosecThread.ps_tty = CONSOLE_TTY;

    task_struct *task = &theCosecThread.ps_task;

    task->kstack = kern_stack;
    task->kstack_size = KERN_STACK_SIZE;
    task->entry = kshell_run;

    /* This esp0 is pointing to the stack this function uses.
     * It's possible to step on our own toes here.
     * TODO: figure out a safe offset. */
    void *esp0 = task->kstack + task->kstack_size - 0x100;
    logmsgdf("%s: esp0 = *%x\n", __func__, esp0);
    task_kthread_init(task, task->entry, esp0);
    task->tss.cr3 = (uintptr_t)pagedir;

    process_attach_tty(&theCosecThread, "/dev/tty0");

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
    cosecd_setup(PID_COSECD);

    theCurrentPID = PID_COSECD;

    tasks_setup(&theCosecThread.ps_task);   // noreturn, must be last
    logmsgef("%s: unreachable", __func__);  // a load-bearing logmsgef
}

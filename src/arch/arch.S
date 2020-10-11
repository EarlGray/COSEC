#define NOT_CC

#include <mem/paging.h>
#include <machine/setjmp.h>

.text

#define SIZEOFINT 4

/*
 *  x86 semaphores
 *
 *  int i386_sema_down(atomic_t semcounter, int block_callback(atomic_t, void *));
 *  int i386_sema_up(atomic_t semcounter, int release_callback(atomic_t, void *));
 */
.global i386_sema_down
.global i386_sema_up

i386_sema_down:
    movl 8(%esp), %eax      # block callback
    movl 4(%esp), %edx      # semaphore counter
    lock incl (%edx)
    jns 1f
    jmp *%eax              # can't acquire
1:  ret

i386_sema_up:
    movl 8(%esp), %eax      # wake callback
    movl 4(%esp), %edx      # semaphore counter
    lock decl (%edx)
    jns 1f
    jmp *%eax              # wake up blocked threads
1:  ret


/*
 *      Setjmp/longjmp
 * int setjmp(jmp_buf env)
 * int longjmp(jmp_buf env, val)
 */
.global i386_setjmp
.global i386_longjmp

i386_setjmp:
    movl 4(%esp), %edx      # &jmp_buf
    movl (%esp),  %ecx      # ret addr

    # save registers protected by cdecl
    movl %ecx, (SIZEOFINT * JMPBUF_EIP_OFFSET)(%edx)
    movl %esp, (SIZEOFINT * JMPBUF_ESP_OFFSET)(%edx)
    movl %ebx, (SIZEOFINT * JMPBUF_EBX_OFFSET)(%edx)
    movl %esi, (SIZEOFINT * JMPBUF_ESI_OFFSET)(%edx)
    movl %edi, (SIZEOFINT * JMPBUF_EDI_OFFSET)(%edx)
    movl %ebp, (SIZEOFINT * JMPBUF_EBP_OFFSET)(%edx)

    xor %eax, %eax
    ret

i386_longjmp:
    movl 4(%esp), %edx          # &jmp_buf
    movl 8(%esp), %eax          # val

    # restore saved regisers
    movl (SIZEOFINT * JMPBUF_EBP_OFFSET)(%edx), %ebp
    movl (SIZEOFINT * JMPBUF_EDI_OFFSET)(%edx), %edi
    movl (SIZEOFINT * JMPBUF_ESI_OFFSET)(%edx), %esi
    movl (SIZEOFINT * JMPBUF_EBX_OFFSET)(%edx), %ebx
    movl (SIZEOFINT * JMPBUF_ESP_OFFSET)(%edx), %esp
    movl (SIZEOFINT * JMPBUF_EIP_OFFSET)(%edx), %ecx  # return address

    movl %edx, 4(%esp)
    movl %ecx, (%esp)

    movl $1, %ecx
    test %eax, %eax
    cmovzl %ecx, %eax  # set return value to 1 if received val==0

    ret

/***
  *     CPUID
  *  bool i386_cpuid_check(int cmd);
  *  void i386_cpuid_vendor(char vendor[12]);
 ***/
.global i386_cpuid_check
i386_cpuid_check:
    pushf
    pushf
    xorl $(1 << 21), (%esp)    # invert ID flag
    popf
    pushf
    pop %eax
    xor (%esp), %eax           # set %eax to changed bits
    popf
    andl $(1 << 21), %eax      # test if ID bit is changed
    ret

.global i386_cpuid_info
i386_cpuid_info:
    push %ebx
    movl 0xc(%esp), %eax
    cpuid
    push %eax
    movl 0xc(%esp), %eax         # char vendor[12]
    movl %ebx, 0(%eax)
    movl %edx, 4(%eax)
    movl %ecx, 8(%eax)
    pop %eax
    pop %ebx
    ret

/***
  *     Paging
 ***/
.global i386_switch_pagedir
i386_switch_pagedir:
    # the first parameter: physical address of the new page directory:
    movl 4(%esp), %eax

    /*
     *   NB: DO NOT disable and re-enable paging around this switch.
     *   CPU still needs to fetch the instructions from their virtual addresses
     *      even if there are no data memory accesses.
     *   This also flushes TLB, no need for INVPLG here.
     */
    movl %eax, %cr3
    ret

.global i386_flush_tlb
i386_flush_tlb:
    movl %cr3, %eax
    movl %eax, %cr3
    ret


/***
  *     Misc
 ***/
.global i386_snapshot
i386_snapshot:
    movl 4(%esp), %edi
    pusha
    movl %esp, %esi
    movl $8, %ecx
    rep movsl
    popa
    ret

.global i386_rdtsc
i386_rdtsc:
    movl 4(%esp), %edi
    rdtsc
    movl %eax, (%edi)
    movl %edx, 4(%edi)
    ret

.global i386_iret
i386_iret:
    movw $043, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    addl $4, %esp /* discard %eip */
    iret

.global test_syscall
test_syscall:
    pushl %ebx
    movl 20(%esp), %ebx
    movl 16(%esp), %edx
    movl 12(%esp), %ecx
    movl 8(%esp), %eax
    int $0x80
    popl %ebx
    ret

#define NOT_CC  1

#include <machine/setjmp.h>

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
    movl %ecx, (4 * JMPBUF_EIP_OFFSET)(%edx)
    movl %esp, (4 * JMPBUF_ESP_OFFSET)(%edx)
    movl %ebx, (4 * JMPBUF_EBX_OFFSET)(%edx)
    movl %esi, (4 * JMPBUF_ESI_OFFSET)(%edx)
    movl %edi, (4 * JMPBUF_EDI_OFFSET)(%edx)
    movl %ebp, (4 * JMPBUF_EBP_OFFSET)(%edx)

    xor %eax, %eax
    ret

i386_longjmp:
    movl 4(%esp), %edx          # &jmp_buf
    movl 8(%esp), %eax          # val

    # restore saved regisers
    movl (4 * JMPBUF_EBP_OFFSET)(%edx), %ebp
    movl (4 * JMPBUF_EDI_OFFSET)(%edx), %edi
    movl (4 * JMPBUF_ESI_OFFSET)(%edx), %esi
    movl (4 * JMPBUF_EBX_OFFSET)(%edx), %ebx
    movl (4 * JMPBUF_ESP_OFFSET)(%edx), %esp
    movl (4 * JMPBUF_EIP_OFFSET)(%edx), %ecx  # return address

    movl %edx, 4(%esp)
    movl %ecx, (%esp)

    movl $1, %ecx
    test %eax, %eax
    cmovzl %ecx, %eax  # set return value to 1 if received val==0

    ret

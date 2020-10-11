#define NOT_CC  1

#include <cosec/syscall.h>

.text

.extern main

.global start
.global _start

_start:
start:
    # TODO: env
    # TODO: args
    call main

    # exit
    movl %eax, %ecx         # exit status
    movl $SYS_EXIT, %eax    # SYS_EXIT 
    int $0x80

1:  jmp 1b                  # should be unreachable

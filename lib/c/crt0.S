#define NOT_CC  1

.text

.extern main
.extern _init
.extern _exit
.extern environ

.global start, _start
.weak start, _start
_start:
start:
    pushl %esp
    call _init

    movl 4(%esp), %eax      // argc
    leal 8(%esp), %edx      // argv
    movl environ, %ecx
    pushl %ecx           // environ
    pushl %edx
    pushl %eax
    call main

    pushl %eax
    call _exit

    int $6   // "invalid operation", should be unreachable

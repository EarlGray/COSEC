#define NOT_CC  1

.text

.extern main
.extern _init
.extern _exit

.global start, _start
.weak start, _start
_start:
start:
    pushl %esp
    call _init

    call main

    pushl %eax
    call _exit

    int $6   // "invalid operation", should be unreachable

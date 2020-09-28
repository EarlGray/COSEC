#include <stdarg.h>
#include <cosec/fs.h>
#include <cosec/syscall.h>

int printf(const char *fmt, ...) {
    return syscall(SYS_PRINT, (int)&fmt, 0, 0);
}

void exit(int status) {
    syscall(SYS_EXIT, status, 0, 0);
}

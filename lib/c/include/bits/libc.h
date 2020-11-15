#ifndef __COSEC_LIBC_H__
#define __COSEC_LIBC_H__

#include <stdint.h>

#define UKNERR_BUF_LEN 40

void *kmalloc(size_t);
void *krealloc(void *ptr, size_t size);
int kfree(void *);

void panic(const char *msg);

/* TODO: gather all the mutable state here */
struct libc {
    int errno;

    size_t unknown_err_len;
    char unknown_error[UKNERR_BUF_LEN];
};
//extern struct libc theLibC;

extern int theErrNo;
extern uint8_t _end;

#endif  // __COSEC_LIBC_H__

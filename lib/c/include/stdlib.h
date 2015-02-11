#ifndef __COSEC_STDLIB_H
#define __COSEC_STDLIB_H

#include <stdint.h>

#define NULL ((void *)0)

void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);

int atoi(const char *nptr);

void *kmalloc(size_t);
int kfree(void *);

void abort(void);

int abs(int i);

#define tmalloc(_type)  ((_type *) kmalloc(sizeof(_type)))

#define null ((void *)0)
#define NULL ((void *)0)

#endif //__COSEC_STDLIB_H

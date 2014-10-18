#ifndef __KHEAP_H__
#define __KHEAP_H__

#include <stdint.h>

/***
  *     Allocate memory
  *   return null if failed
 ***/
void *kmalloc(size_t size);

/***
  *     Free memory
 ***/
int kfree(void *p);

void kheap_setup(void);
void kheap_info(void);

void * kheap_check(void);

#endif // __KHEAP_H__

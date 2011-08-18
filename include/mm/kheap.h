#ifndef __KHEAP_H__
#define __KHEAP_H__

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

#endif // __KHEAP_H__

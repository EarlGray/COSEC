#ifndef __FIRSTFIT_H__
#define __FIRSTFIT_H__

/***
  *     Use a pointer to this structure as identifier of specific memory area 
  *    being managed, e.g. struct firstfit_allocator *heap1, *heap2;  (memory 
  *    areas must not be interleaved).
 ***/
struct firstfit_allocator;

/***
  *      Create  a memory manager  which manages  a memory area  starting at 
  *     'startmem' with size 'size'. Return 'null', if fails (e.g.size is not 
  *     sufficient for storing at least 1 byte). 
 ***/
struct firstfit_allocator * firstfit_new(void *startmem, unsigned size);

/***
  *     Allocate 'length' byte area in 'this'
  *    Returns 'null', if allocation is impossible.
 ***/
void *firstfit_malloc(struct firstfit_allocator *this, unsigned size);

/***
  *     Free memory, keep consistency of the heap, check heap for corruption, 
  *    try to repair it if necessary
 ***/
void firstfit_free(struct firstfit_allocator *this, void *p);

void *firstfit_corruption(struct firstfit_allocator *this);
void heap_info(struct firstfit_allocator *this);

#endif // __FIRSTFIT_H__

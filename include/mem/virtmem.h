#ifndef __VIRTMEM_H__COSEC__
#define __VIRTMEM_H__COSEC__

#define VM_RW       (1 << 1)
#define VM_USR      (1 << 2)
#define VM_XD       (uint64_t)(1 << 63)

typedef struct vm_area vm_area_t;

void memory_setup(void);

#endif // __VIRTMEM_H__COSEC__

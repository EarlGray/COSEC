#ifndef __CONF_H__
#define __CONF_H__

#define PAGE_SIZE       0x1000

#define PAGING          (1)

#define INTR_PROFILING  (0)
#define MEM_DEBUG       (1)
#define TASK_DEBUG      (0)
#define INTR_DEBUG      (1)

#if PAGING
# define KERN_OFF       0xc0000000
#else
# define KERN_OFF       0x00000000
#endif

#define KERN_PA         0x00100000

#endif // __CONF_H__

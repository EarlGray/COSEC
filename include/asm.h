#ifndef __ASM_H
#define __ASM_t

#define thread_hang()	\
	asm volatile ("hang:	hlt\n\tjmp hang\n" ::)

#define lidt(ptr)	\
	__asm__ __volatile__ ("lidt (%%eax)\n" : : "r"((void *)(ptr)))

#define lgdt(ptr)	\
	__asm__ __volatile__ ("lgdt (%%eax)\n" : : "r"((void *)(ptr)))

#define inb(port, value) \
    asm volatile ("inb %%dx,%%al\n": "=a"(value): "d"(port))

#define outb(port, value) \
    asm volatile ("outb %%al,%%dx\n"::"a" (value),"d" (port))

#define io_wait() \
    asm ("\tjmp 1f\n1:\tjmp 1f\n1:") 



#endif // __ASM_H

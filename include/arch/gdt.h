#ifndef __MEMORY_H
#define __MEMORY_H

/*
 *      This file provides basic structures of Intel CPUs:
 *   - privilige levels used by the OS.
 *   - segment descriptor structure
 *   - segment selector structure,
 *   - basic selectors constants;
 */

/* GDT indeces */
#define GDT_DUMMY       0
#define GDT_KERN_CS     1
#define GDT_KERN_DS     2
#define GDT_USER_CS     3
#define GDT_USER_DS     4

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

extern void gdt_get(struct gdt_ptr *idt);

#define SEL_KERN_CS     ((GDT_KERN_CS << 3) + (PL_KERN << 1))
#define SEL_KERN_DS     ((GDT_KERN_DS << 3) + (PL_KERN << 1))
#define SEL_USER_CS     ((GDT_USER_CS << 3) + (PL_USER << 1))
#define SEL_USER_DS     ((GDT_USER_DS << 3) + (PL_USER << 1))

void gdt_setup(void);
void gdt_info(void);

#endif // __MEMORY_H

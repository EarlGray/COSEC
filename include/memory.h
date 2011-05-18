#ifndef __MEMORY_H
#define __MEMORY_H

/*
 *      This file provides basic structures of Intel CPUs:
 *   - privilige levels used by the OS.
 *   - segment descriptor structure
 *   - segment selector structure,
 *   - basic selectors constants;
 */

#include <misc.h>

//	privilege levels
enum privilege_level {
    PL_KERN = 0,
    PL_USER = 3
};

#define SEGDESC_BYTE_GRANULARITY	0
#define SEGDESC_4KB_GRANULARITY		1

enum segdesc {
	CODE_SEGDESC,
	DATA_SEGDESC,
	SYS_SEGDESC,
};

enum segdesc_type {
#define SDTP_EXTANDABLE 0x04       
#define SDTP_CODE       0x08    // data otherwise
#define SDTP_CONFORMING 0x04
#define SDTP_CODE_READ  0x02    // execute-only code otherwise
#define SDTP_DATA_WRITE 0x02    // readonly-data otherwise
#define SDTP_ACCESED    0x01    
	SEGDESC_TYPE_ER_CODE = SDTP_CODE | SDTP_CODE_READ,
	//SEGDESC_TYPE_EO_CODE = 
	//SEGDESC_TYPE_RO_DATA = 
	SEGDESC_TYPE_RW_DATA = SDTP_DATA_WRITE,
	//SEGDESC_TYPE_STACK = 
};

struct segdescr {
	uint16_t	limit_l;	// lower part
	uint16_t	base_l;	// base lower 
	uint8_t		base_m;	// base middle
	uint8_t		type:4;	
	uint8_t		system_bit:1;
	uint8_t		dpl:2;
	uint8_t		mem_present:1;
    uint8_t		limit_h:4;
	uint8_t		user:1;     // custom bit
	uint8_t		digit64:1;  // 1 -> 64bit        
	uint8_t		digit:1;    // 1 -> 32bit
	uint8_t		granularity:1;
	uint8_t		base_h;
};

enum gdt_sels {
    GDT_DUMMY   =   0,
    GDT_KERN_CS,
    GDT_KERN_DS,
    GDT_USER_CS,
    GDT_USER_DS,
};

struct selector {
    uint8_t  local_bit:1;
    uint8_t  dpl:2;
    uint16_t index:13;
};

struct regs {
    uint16_t es, ds;                                // saved
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;// pushed by 'pusha'
    uint32_t int_no, err_code;                      // by isr entries / CPU
    unsigned int eip, cs, eflags, useresp, ss;      // by CPU 
};

extern const struct selector SEL_KERN_CS;
extern const struct selector SEL_KERN_DS;
extern const struct selector SEL_USER_CS;
extern const struct selector SEL_USER_DS;

void memory_setup(void);

#endif // __MEMORY_H

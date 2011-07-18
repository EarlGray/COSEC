#ifndef __CPU_H__
#define __CPU_H__

typedef enum {
    PL_KERN = 0,
    PL_USER = 3
} privilege_level;

#define SD_GRAN_4Kb     1
#define SD_GRAN_1b      0

/* SDTP = Segment Descriptor TyPe */
#define SDTP_CODE       0x08    // data otherwise
#define SDTP_CONFORMING 0x04    // code conforming
#define SDTP_EXPANDDOWN 0x04    // data expand down
#define SDTP_CODE_READ  0x02    // execute-only code otherwise
#define SDTP_DATA_WRITE 0x02    // readonly-data otherwise
#define SDTP_ACCESED    0x01
#define SDTP_TSS_BUSY   0x02

/* sysbit cleared: data/code segdescr */
#define SD_TYPE_ER_CODE         SDTP_CODE | SDTP_CODE_READ
#define SD_TYPE_EO_CODE         SDTP_CODE 
#define SD_TYPE_RO_DATA         0
#define	SD_TYPE_RW_DATA         SDTP_DATA_WRITE

/* sysbit set: LDT/gate/TSS */
#define SD_TYPE_LDT             0x2
#define SD_TYPE_TASK_GATE       0x5
#define SD_TYPE_TSS             0x9
#define SD_TYPE_TSS_BUSY        SD_TYPE_TSS | SDTP_TSS_BUSY
#define SD_TYPE_CALL_GATE       0xC
#define SD_TYPE_INTR_GATE       0xE
#define SD_TYPE_TRAP_GATE       0xF


typedef struct segdescr {
    union {
        uint32_t ints[2];
        struct {
            uint16_t	limit_l;	// lower part
            uint16_t	base_l;	// base lower 
            uint8_t		base_m;	// base middle
            uint8_t	    type:4;	
            uint8_t		sysbit:1;
            uint8_t		dpl:2;
            uint8_t		mem_present:1;
            uint8_t		limit_h:4;
            uint8_t		user:1;     // custom bit
            uint8_t		digit64:1;  // 1 -> 64bit        
            uint8_t		digit:1;    // 1 -> 32bit
            uint8_t		granularity:1;
            uint8_t		base_h;
        } strct;
    } as;
} segment_descriptor;

#define segdescr_base(seg)      ( ((seg)->base_h << 24) | ((seg)->base_m << 16) | (seg)->base_l )
#define segdescr_limit(seg)     ( ((seg)->limit_h << 16) | (seg)->limit_l )

#define segdescr_usual_init(seg, type, limit, base, dpl, gran) {   \
    (seg).as.ints[0] = ((base) << 16) | ((limit) & 0xFFFF);                               \
    (seg).as.ints[1] = (((base) >> 16) & 0xFF) | ((type) << 8) | ((dpl) << 13) | 0x9000;  \
    (seg).as.ints[1] |=                                                                   \
     ( (((limit) >> 16) & 0x0F) | (((base) >> 16) & 0xFF00) | (gran << 7) | 0x40) << 16;  \
}

#define segdescr_gate_init(seg, slctr, addr, dpl, trap) {   \
    (seg).as.ints[0] = ((uint16_t)(slctr) << 16) | (addr & 0x0000FFFF);                  \
    (seg).as.ints[1] = (addr & 0xFFFF0000) | 0x8E00 | ((dpl) << 13);                     \
    if (trap) (seg).as.ints[1] |= 0x0100;                                                 \
}

#define segdescr_taskgate_init(seg, tasksel, dpl) {                                       \
    (seg).as.ints[0] = ((uint16_t)(tasksel)) << 16;                                       \
    (seg).as.ints[1] = 0x8500 | ((dpl) << 13);                                            \
}

typedef struct {
    union {
        uint16_t word;
        struct {
            uint8_t  local_bit:1;
            uint8_t  dpl:2;
            uint16_t index:13;
        } strct;
    } as;
} segment_selector;

#define segsel_index(ss)        ((ss) >> 3)
#define rpl(ss)                 ((ss >> 1) & 0x3)

#define arch_memcpy(dst, src, size) {       \
    asm("movl %0, %%edi \n" : : "r"(dst));  \
    asm("movl %0, %%esi \n" : : "r"(src));  \
    asm("movl %0, %%ecx \n" : : "r"(size)); \
    asm("rep movsb");                       \
}

#define arch_strncpy(dst, src, n) {         \
    asm("movl %0, %%edi \n" : : "r"(dst));  \
    asm("movl %0, %%esi \n" : : "r"(src));  \
    asm("movl %0, %%ecx \n" : :"r"(n));     \
    asm("repnz movsb");                     \
}

#define thread_hang()   asm volatile ("1:    hlt\n\tjmp 1b\n" ::)
        
#define io_wait()       asm ("\tjmp 1f\n1:\tjmp 1f\n1:") 

#define inb(port, value)    asm volatile ("inb %%dx,%%al\n": "=a"(value): "d"(port))
#define outb(port, value)   asm volatile ("outb %%al,%%dx\n"::"a" (value),"d" (port))

#define inw(port, value)    asm volatile ("inw %%dx,%%ax\n": "=a"(value): "d"(port))
#define outw(port, value)   asm volatile ("outw %%ax,%%dx\n"::"a" (value),"d" (port))

#define inl(port, value)    asm volatile ("inl %%dx,%%eax\n": "=a"(value): "d"(port))
#define outl(port, value)   asm volatile ("outl %%eax,%%dx\n"::"a" (value),"d" (port))


#define inb_p(port, value)  do { inb(port, value); io_wait();  } while (0)
#define outb_p(port, value) do { outb(port, value); io_wait();  } while (0)

#define inw_p(port, value)  do { inw(port, value); io_wait();  } while (0)
#define outw_p(port, value) do { outw(port, value); io_wait();  } while (0)

#define inl_p(port, value)  do { inl(port, value); io_wait();  } while (0)
#define outl_p(port, value) do { outl(port, value); io_wait();  } while (0)

#define intrs_enable()  asm ("\t sti \n")
#define intrs_disable() asm ("\t cli \n")                                                       
#define cpu_halt()      asm ("\t hlt \n")

#define stack_pointer(p)    asm ("\t movl %%esp, %0 \n" : "=r"(p))


#define cpu_snapshot(buf) {    \
    uint32_t stack;                             \
    asm(" movl %%esp, %0 \n" : "=r"(stack));    \
    asm(" pusha \n");                           \
    asm("movl %0, %%edi \n" : : "r"(buf));      \
    asm("movl %%esp, %%esi \n" );               \
    asm("movl %0, %%ecx \n" : : "r"(100));      \
    asm("rep movsb");                           \
    asm(" movl %0, %%esp \n" : : "r"(stack));   \
}

#define eflags(flags) {     \
    asm ("\t pushf \n");    \
    asm ("\t movl (%%esp), %0 \n" : "=r"(flags));   \
    asm ("\t popf \n");     \
}


#endif // __CPU_H__

#ifndef __CPU_H__
#define __CPU_H__

typedef enum {
    PL_KERN = 0,
    PL_USER = 3
} privilege_level;

typedef enum gatetype {
    GATE_INTR,      // Intel: interrupt gate, dpl = 0   Linux: intrgate
    GATE_TRAP,      // Intel: trap gate,    dpl = 0     Linux: trapgate
    GATE_CALL,      // Intel: trap gate,    dpl = 3     Linux: sysgate
} i386_gate_t;

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

#define make_selector(index, ti, pl)  (((index & 0xFFFF) << 3) + ((ti & 1) << 2) + (pl & 3))
#define segsel_index(ss)        ((ss) >> 3)
#define rpl(ss)                 ((ss >> 1) & 0x3)

#define i386_memcpy(dst, src, size) {       \
    asm("movl %0, %%edi \n" : : "r"(dst));  \
    asm("movl %0, %%esi \n" : : "r"(src));  \
    asm("movl %0, %%ecx \n" : : "r"(size)); \
    asm("rep movsb");                       \
}

#define i386_strncpy(dst, src, n) {         \
    asm("movl %0, %%edi \n" : : "r"(dst));  \
    asm("movl %0, %%esi \n" : : "r"(src));  \
    asm("movl %0, %%ecx \n" : :"r"(n));     \
    asm("repnz movsb");                     \
}

#define i386_hang()   asm volatile ("1:    hlt\n\tjmp 1b\n" ::)
        
#define io_wait()       asm ("\tjmp 1f\n1:\tjmp 1f\n1:") 

#define i386_inb(port, value)    asm volatile ("inb %%dx,%%al\n": "=a"(value): "d"(port))
#define i386_outb(port, value)   asm volatile ("outb %%al,%%dx\n"::"a" (value),"d" (port))
#define i386_inw(port, value)    asm volatile ("inw %%dx,%%ax\n": "=a"(value): "d"(port))
#define i386_outw(port, value)   asm volatile ("outw %%ax,%%dx\n"::"a" (value),"d" (port))
#define i386_inl(port, value)    asm volatile ("inl %%dx,%%eax\n": "=a"(value): "d"(port))
#define i386_outl(port, value)   asm volatile ("outl %%eax,%%dx\n"::"a" (value),"d" (port))

#define i386_inb_p(port, value)  do { inb(port, value); io_wait();  } while (0)
#define i386_outb_p(port, value) do { outb(port, value); io_wait();  } while (0)
#define i386_inw_p(port, value)  do { inw(port, value); io_wait();  } while (0)
#define i386_outw_p(port, value) do { outw(port, value); io_wait();  } while (0)
#define i386_inl_p(port, value)  do { inl(port, value); io_wait();  } while (0)
#define i386_outl_p(port, value) do { outl(port, value); io_wait();  } while (0)

#define i386_intrs_enable()  asm ("\t sti \n")
#define i386_intrs_disable() asm ("\t cli \n")                                                       
#define i386_halt()      asm ("\t hlt \n")

#define i386_esp(p)    asm ("\t movl %%esp, %0 \n" : "=r"(p))


extern void i386_snapshot(char *buf);

struct pusha {
    uint ss, gs, fs, es, ds;
    uint edi, esi, ebp, esp;
    uint ebx, edx, ecx, eax;
};

#define i386_eflags(flags) {     \
    asm ("\t pushf \n");    \
    asm ("\t movl (%%esp), %0 \n" : "=r"(flags));   \
    asm ("\t popf \n");     \
}


/* GDT indeces */
#define GDT_DUMMY       0
#define GDT_KERN_CS     1
#define GDT_KERN_DS     2
#define GDT_USER_CS     3
#define GDT_USER_DS     4
#define GDT_DEFAULT_LDT 5

#define SEL_KERN_CS     make_selector(GDT_KERN_CS, 0, PL_KERN)
#define SEL_KERN_DS     make_selector(GDT_KERN_DS, 0, PL_KERN)
#define SEL_USER_CS     make_selector(GDT_USER_CS, 0, PL_USER)
#define SEL_USER_DS     make_selector(GDT_USER_DS, 0, PL_USER)
#define SEL_DEFAULT_LDT make_selector(GDT_DEFAULT_LDT, 0, PL_KERN)

segment_descriptor * i386_gdt(void);
segment_descriptor * i386_idt(void);

index_t gdt_alloc_entry(segment_descriptor entry);


/***
  *     Task-related definitions
 ***/

struct task_state_seg {
    uint prev_task_link;    // high word is reserved

    uint esp0;
    uint ss0;           // high word is reserved
    uint esp1;
    uint ss1;           // high word is reserved
    uint esp2;
    uint ss2;           // high word is reserved

    uint cr3;
  
    uint eip;
    uint eflags;  

    uint eax, ecx, edx, ebx;
    uint esp, ebp, esi, edi;

    uint es, cs, ss, ds, fs, gs;
    uint ldt;
    uint io_map_addr;
};
typedef  struct task_state_seg  tss_t;


/***
  *     Common-architecture interface
 ***/

ptr_t cpu_stack(void);

#define intrs_enable()         i386_intrs_enable()
#define intrs_disable()        i386_intrs_disable()
#define cpu_halt()             i386_halt()
#define thread_hang()          i386_hang()

#define inb(port, value)       i386_inb(port, value)    
#define outb(port, value)      i386_outb(port, value)   
#define inw(port, value)       i386_inw(port, value)    
#define outw(port, value)      i386_outw(port, value)   
#define inl(port, value)       i386_inl(port, value)    
#define outl(port, value)      i386_outl(port, value)   

#define inb_p(port, value)     i386_inb_p(port, value) 
#define outb_p(port, value)    i386_outb_p(port, value)
#define inw_p(port, value)     i386_inw_p(port, value) 
#define outw_p(port, value)    i386_outw_p(port, value)
#define inl_p(port, value)     i386_inl_p(port, value) 
#define outl_p(port, value)    i386_outl_p(port, value)


#define arch_strncpy(dst, src, n)   i386_strncpy(dst, src, n)
#define arch_memcpy(dst, src, size) i386_memcpy(dst, src, size) 

void cpu_setup(void);

#endif // __CPU_H__
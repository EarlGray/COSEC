#ifndef __CPU_H__
#define __CPU_H__

#include <stdint.h>
#include <attrs.h>

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
#define SD_TYPE_RW_DATA         SDTP_DATA_WRITE

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
        uint64_t ll;
        struct {
            uint16_t    limit_l;    // lower part
            uint16_t    base_l;     // base lower
            uint8_t     base_m;     // base middle
            uint8_t     type:4;
            uint8_t     sysbit:1;   // systemseg or code/dataseg
            uint8_t     dpl:2;
            uint8_t     mem_present:1;
            uint8_t     limit_h:4;
            uint8_t     user:1;     // custom bit
            uint8_t     digit64:1;  // 1 -> 64bit
            uint8_t     digit:1;    // 1 -> 32bit
            uint8_t     granularity:1;
            uint8_t     base_h;
        } strct;
    } as;
} segment_descriptor;

static inline uint32_t segdescr_base(segment_descriptor seg) {
    return (seg.as.strct.base_h << 24) | (seg.as.strct.base_m << 16) | seg.as.strct.base_l;
}
static inline uint32_t segdescr_limit(segment_descriptor seg) {
    return (seg.as.strct.limit_h << 16) | seg.as.strct.limit_l;
}

#define segdescr_usual_init(seg, type, limit, base, dpl, gran) {   \
    (seg).as.ints[0] = ((base) << 16) | ((limit) & 0xFFFF);                               \
    (seg).as.ints[1] = (((base) >> 16) & 0xFF) | ((type) << 8) | ((dpl) << 13) | 0x9000;  \
    (seg).as.ints[1] |=                                                                   \
     ( (((limit) >> 16) & 0x0F) | (((base) >> 16) & 0xFF00) | (gran << 7) | 0x40) << 16;  \
}

#define segdescr_gate_init(seg, slctr, addr, dpl, trap) {               \
    (seg).as.ints[0] = ((uint16_t)(slctr) << 16) | (addr & 0x0000FFFF); \
    (seg).as.ints[1] = (addr & 0xFFFF0000) | 0x8E00 | ((dpl) << 13);    \
    if (trap) (seg).as.ints[1] |= 0x0100;                               \
}

#define segdescr_taskgate_init(seg, tasksel, dpl) {     \
    (seg).as.ints[0] = ((uint16_t)(tasksel)) << 16;     \
    (seg).as.ints[1] = 0x8500 | ((dpl) << 13);          \
}

#define segdescr_taskstate_init(seg, addr, dpl) {                    \
    (seg).as.ints[0] = (0x67 /*limit*/ | ((addr & 0xFFFF) << 16));   \
    (seg).as.ints[1] = ((addr & 0xFF000000) | ((addr >> 16) & 0xFF)  \
            | 0x00808900 | ((dpl & 0x3) << 13));                     \
}

#define segdescr_taskstate_busy(seg, busy_flag) {       \
    if (busy_flag) (seg).as.ints[1] |= (1 << 9);        \
    else (seg).as.ints[1] &= ~(1u << 9);                \
}

typedef struct {
    union {
        uint16_t word;
        struct {
            uint8_t  dpl:2;
            uint8_t  local_bit:1;
            uint16_t index:13;
        } strct;
    } as;
} segment_selector;

/* table indicator, ti */
#define SEL_TI_GDT  0
#define SEL_TI_LDT  1

#define make_selector(index, ti, pl) \
    ((((index) & 0xFFFF) << 3) + (((ti) & 1) << 2) + ((pl) & 3))

static inline segment_selector make_segment_selector(uint16_t index, uint8_t ti, uint8_t pl) {
    segment_selector sel;
    sel.as.word = ((((index) & 0xFFFF) << 3) + (((ti) & 1) << 2) + ((pl) & 3));
    return sel;
};

#define segsel_index(ss)        ((ss) >> 3)
#define rpl(ss)                 ((ss >> 1) & 0x3)

#define i386_memcpy(dst, src, size) \
    asm(                    \
    "movl %0, %%edi     \n" \
    "movl %1, %%esi     \n" \
    "movl %2, %%ecx     \n" \
    "rep movsb          \n" \
    : : "m"(dst),"m"(src),"r"(size) )

#define n_insw(port, buf, count) \
    asm(                   \
    "movw %0, %%cx      \n"\
    "movl %1, %%edi     \n"\
    "movw %2, %%dx      \n"\
    "rep insw           \n"\
    :: "n"(count), "r"(buf), "m"(port))

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

static inline void i386_load_task_reg(segment_selector sel) {
    asm ("ltrw %%ax     \n\t"::"a"( sel.as.word ));
}

static inline uint16_t i386_store_task_reg(void) {
    uint16_t ret;
    asm ("strw %%ax     \n\t" : "=a"(ret));
    return ret;
}

extern uint i386_rdtsc(uint64_t *timestamp);
extern void i386_snapshot(char *buf);

static inline uint64_t i386_read_msr(uint32_t msr) {
    uint32_t eax = 0, edx = 0;
    asm("rdmsr\n" : "=c"(msr) : "a"(eax), "d"(edx));
    return (((uint64_t)edx) << 32) | (uint64_t)eax;
}

static inline uint32_t i386_eflags(void) {
    uint32_t flags;
    asm("pushf              \n\t"   \
        "movl (%%esp), %0   \n\t"   \
        "popf               \n\t"   \
        : "=r"(flags));
    return flags;
}

#define EFL_RSRVD   (1 <<  1)
#define EFL_IF      (1 <<  9)
#define EFL_NT      (1 << 14)
#define EFL_VM      (1 << 17)
#define EFL_IOPL3   (PL_USER << 12)

struct eflags {
    uint8_t cf:1;   // 0
    uint8_t :1;
    uint8_t pf:1;
    uint8_t :1;
    uint8_t af:1;   // 4
    uint8_t :1;
    uint8_t zf:1;
    uint8_t sf:1;

    uint8_t tf:1;   // 8
    uint8_t ifl:1;
    uint8_t df:1;
    uint8_t of:1;
    uint8_t iopl:2; // 12
    uint8_t nt:1;
    uint8_t :1;

    uint8_t rf:1;   // 16
    uint8_t vm:1;
    uint8_t ac:1;
    uint8_t vif:1;
    uint8_t vip:1;  // 20
    uint8_t id:1;
};

#define eflags_iopl(pl)     ((pl & 3) << 12)

uint32_t x86_eflags(void);

/* GDT indeces */
#define GDT_DUMMY       0
#define GDT_KERN_CS     1
#define GDT_KERN_DS     2
#define GDT_USER_CS     3
#define GDT_USER_DS     4
#define GDT_DEF_LDT     5

#define SEL_KERN_CS     make_selector(GDT_KERN_CS, SEL_TI_GDT, PL_KERN)
#define SEL_KERN_DS     make_selector(GDT_KERN_DS, SEL_TI_GDT, PL_KERN)
#define SEL_USER_CS     make_selector(GDT_USER_CS, SEL_TI_GDT, PL_USER)
#define SEL_USER_DS     make_selector(GDT_USER_DS, SEL_TI_GDT, PL_USER)
#define SEL_DEF_LDT     make_selector(GDT_DEF_LDT, SEL_TI_GDT, PL_USER)

segment_descriptor * i386_gdt(void);
segment_descriptor * i386_idt(void);

index_t gdt_alloc_entry(segment_descriptor entry);

/* as laid out in memory by PUSHA */
struct i386_general_purpose_registers {
    uint edi, esi, ebp, esp;
    uint ebx, edx, ecx, eax;
};
typedef  struct i386_general_purpose_registers  i386_gp_regs;

static uint8_t i386_current_privlevel(void) {
    uint16_t cs_sel;
    asm("movw %%cs, %0 \n" : "=r"(cs_sel));
    return cs_sel & 0x0003;
}

/***
  *     x86 CPUID
 ***/
int i386_cpuid_check(void);
uint i386_cpuid_info(void *cpu_info, uint funct);

/***
  *     Semaphores
 ***/
int i386_sema_down(int semcounter, int block_callback(int, void *));
int i386_sema_up(int semcounter, int release_callback(int, void *));

/***
  *     Paging
 ***/
extern void i386_switch_pagedir(void *new_pagedir);

/***
  *     Interrupts
 ***/

#define CONTEXT_SIZE        0x30

extern uint32_t  intr_err_code(void);
extern uintptr_t intr_context_esp(void);
extern void  intr_set_context_esp(uintptr_t esp);
extern void  intr_switch_cr3(uintptr_t cr3);

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

    uint io_map1, io_map2;
};
typedef  struct task_state_seg  tss_t;


void i386_iret(uintptr_t eip3, uint cs3, uint32_t eflags, uintptr_t esp3, uint ss3);


/***
  *     Common-architecture interface
 ***/

uintptr_t cpu_stack(void);

#define intrs_enable()         i386_intrs_enable()
#define intrs_disable()        i386_intrs_disable()
#define cpu_halt()             i386_halt()

static void __noreturn cpu_hang(void) { for (;;) i386_halt(); }

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

#include <asm.h>
#include <screen.h>

#include <gdt.h>

/***
  *     Internal declarations
 ***/

const struct selector SEL_KERN_CS = { .index = GDT_KERN_CS, .local_bit = 0, .dpl = PL_KERN };
const struct selector SEL_KERN_DS = { .index = GDT_KERN_DS, .local_bit = 0, .dpl = PL_KERN };
const struct selector SEL_USER_CS = { .index = GDT_USER_CS, .local_bit = 0, .dpl = PL_USER };
const struct selector SEL_USER_DS = { .index = GDT_USER_DS, .local_bit = 0, .dpl = PL_USER };

uint32_t segdescr_base(struct segdescr *seg);
uint20_t segdescr_limit(struct segdescr *seg);

void segdescr_init(struct segdescr *seg, enum segdesc segtype, uint32_t limit, uint32_t base, enum segdesc_type type, uint8_t dpl, uint8_t bits);


/*****************************************************************************
		GDT
******************************************************************************/

#define N_TASKS     40
#define N_GDT       (5 + N_TASKS * 2)
#if (N_GDT > 8191)
    #error "N_TASKS is too big"
#endif

struct segdescr theGDT[N_GDT];

inline void *
memset(void *s, int c, size_t n) {
    char *p = (char *)s;
    unsigned i;
    for (i = 0; i < n; ++i)
        p[i] = c;
    return s;
}

void print_mem(char *p, size_t count) {
    size_t i;
    for (i = 0; i < count; ++i) {
        if (0 == (uint32_t)(p + i) % 0x10) 
            k_printf("\n%x : ", (uint32_t)(p + i));
        int t = (uint8_t) p[i];
        k_printf("%x ", t);
    }
    k_printf("\n");
}

extern void gdt_load(uint16_t limit, void *base);

inline void
segdescr_init(struct segdescr *seg, enum segdesc segtype, uint32_t limit, uint32_t base, enum segdesc_type type, uint8_t dpl, uint8_t granularity) { 
    uint8_t *segm = (uint8_t *) seg;
    uint16_t *segm16 = (uint16_t *) seg;

    segm16[0] = (uint16_t) limit;
    segm16[1] = (uint16_t) base;
    segm[4] = (uint8_t) (base >> 16);
    segm[5] = type | (segtype == SYS_SEGDESC ? 0x00 : 0x10 ) | (dpl << 5) | 0x80;
    segm[6] = (0x0F & (uint8_t)(limit >> 16)) + 0x40 + (granularity << 7);
    segm[7] = (uint8_t) (base >> 24);
}

inline uint32_t 
segdescr_base(struct segdescr *seg) {
    return (seg->base_h << 24) | (seg->base_m << 16) | seg->base_l;
}

inline uint20_t 
segdescr_limit(struct segdescr *seg) {
    return (seg->limit_h << 16) | seg->limit_l;
}

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

extern void gdt_get(struct gdt_ptr *idt);
extern void print_mem(char *, size_t);

void gdt_setup(void) {
    struct gdt_ptr gdt;
    gdt_get(&gdt);
    
    memset(theGDT, 0, N_TASKS * sizeof(struct segdescr));
    segdescr_init(theGDT + SEL_KERN_CS.index, CODE_SEGDESC, 0xFFFFF, 0x0, SEGDESC_TYPE_ER_CODE, PL_KERN, SEGDESC_4KB_GRANULARITY);
    segdescr_init(theGDT + SEL_KERN_DS.index, DATA_SEGDESC, 0xFFFFF, 0x0, SEGDESC_TYPE_RW_DATA, PL_KERN, SEGDESC_4KB_GRANULARITY);
    segdescr_init(theGDT + SEL_USER_CS.index, CODE_SEGDESC, 0xFFFFF, 0x0, SEGDESC_TYPE_ER_CODE, PL_USER, SEGDESC_4KB_GRANULARITY);
    segdescr_init(theGDT + SEL_USER_DS.index, DATA_SEGDESC, 0xFFFFF, 0x0, SEGDESC_TYPE_RW_DATA, PL_USER, SEGDESC_4KB_GRANULARITY);
    //segdescr_init(theGDT + (DEFAULT_LDT >> 3), SYS_SEGDESC, 0xFFFFF, 0x0, SEGDESCR_TYPE_RW_DATA, PL_USER, SEGDESC_4KB_GRANULARITY);

	gdt_load(N_GDT, theGDT);
#ifdef VERBOSE
    k_printf("\nGDT:");
    print_mem((char *)theGDT, 0x40);
#endif
}


#include <asm.h>
#include <kconsole.h>

#include <memory.h>

/**
  *     Internal declarations
  */

uint32_t segdescr_base(segdescr_t *seg);
uint20_t segdescr_limit(segdescr_t *seg);

void segdescr_init(segdescr_t *seg, segdesc_e segtype, uint32_t limit, uint32_t base, segdesc_type_e type, uint8_t dpl, uint8_t bits);


/*****************************************************************************
		GDT
******************************************************************************/

#define N_TASKS     40
#define N_GDT       (5 + N_TASKS * 2)
#if (N_GDT > 8191)
    #error "N_TASKS is too big"
#endif

segdescr_t theGDT[N_GDT];

inline void *
memset(void *s, int c, size_t n) {
    char *p = (char *)s;
    unsigned i;
    for (i = 0; i < n; ++i)
        p[i] = c;
    return s;
}

inline void
gdt_load(void) {
    uint32_t gdtr[3];
    gdtr[0] = N_GDT << 16;
    gdtr[1] = (uint32_t) theGDT;

    lgdt(((char *) gdtr) + 2);
}

inline void
segdescr_init(segdescr_t *seg, segdesc_e segtype, uint32_t limit, uint32_t base, segdesc_type_e type, uint8_t dpl, uint8_t granularity) { 
    uint8_t *segm = (uint8_t *) seg;
    uint16_t *segm16 = (uint16_t *) seg;

    segm16[0] = (uint16_t) limit;
    segm16[1] = (uint16_t) base;
    segm[4] = (uint8_t) (base >> 16);
    segm[5] = type + (segtype == SYS_SEGDESC ? 0x00 : 0x10 ) + (dpl << 5) + 0x80;
    segm[6] = (0x0F & (uint8_t)(limit >> 16)) + 0x40 + (granularity << 7);
    segm[7] = (uint8_t) (base >> 24);
}

inline uint32_t 
segdescr_base(segdescr_t *seg) {
    return (seg->base_h << 24) | (seg->base_m << 16) | seg->base_l;
}

inline uint20_t 
segdescr_limit(segdescr_t *seg) {
    return (seg->limit_h << 16) | seg->limit_l;
}

void memory_setup(void) {
    k_printf("sizeof(segsel) = %d\n", sizeof(segsel_t));
    memset(theGDT, 0, N_TASKS * sizeof(segdescr_t));
    segdescr_init(theGDT + (KERN_CODE_SEG >> 3), CODE_SEGDESC, 0xFFFFF, 0x0, SEGDESC_TYPE_ER_CODE, PL_KERN, SEGDESC_4KB_GRANULARITY);
    segdescr_init(theGDT + (KERN_DATA_SEG >> 3), DATA_SEGDESC, 0xFFFFF, 0x0, SEGDESC_TYPE_RW_DATA, PL_KERN, SEGDESC_4KB_GRANULARITY);
    segdescr_init(theGDT + (USER_CODE_SEG >> 3), CODE_SEGDESC, 0xFFFFF, 0x0, SEGDESC_TYPE_ER_CODE, PL_USER, SEGDESC_4KB_GRANULARITY);
    segdescr_init(theGDT + (USER_DATA_SEG >> 3), DATA_SEGDESC, 0xFFFFF, 0x0, SEGDESC_TYPE_RW_DATA, PL_USER, SEGDESC_4KB_GRANULARITY);
    //segdescr_init(theGDT + (DEFAULT_LDT >> 3), SYS_SEGDESC, 0xFFFFF, 0x0, SEGDESCR_TYPE_RW_DATA, PL_USER, SEGDESC_4KB_GRANULARITY);
	gdt_load();
}


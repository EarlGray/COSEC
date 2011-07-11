#include <asm.h>
#include <mm/gdt.h>

/***
  *     Internal declarations
 ***/
#define N_TASKS     40
#define N_GDT       (5 + N_TASKS * 2)

void segdescr_init(struct segdescr *seg, enum segdesc segtype, uint32_t limit, uint32_t base, enum segdesc_type type, uint8_t dpl, uint8_t bits);

extern void gdt_load(uint16_t limit, void *base);

/*****************************************************************************
		GDT
******************************************************************************/

#if (N_GDT > 8191)
    #error "N_TASKS is too big"
#endif

segment_descriptor theGDT[N_GDT];
/*
inline void
segdescr_init(segment_descriptor *seg, uint8_t segtype, uint32_t limit, uint32_t base, enum segdesc_type type, uint8_t dpl, uint8_t granularity) { 
    uint8_t *segm = (uint8_t *) seg;
    uint16_t *segm16 = (uint16_t *) seg;

    segm16[0] = (uint16_t) limit;
    segm16[1] = (uint16_t) base;
    segm[4] = (uint8_t) (base >> 16);
    segm[5] = type | (segtype == SYS_SEGDESC ? 0x00 : 0x10 ) | (dpl << 5) | 0x80;
    segm[6] = (0x0F & (uint8_t)(limit >> 16)) + 0x40 + (granularity << 7);
    segm[7] = (uint8_t) (base >> 24);
}
*/
#define gdt_entry_init(index, type, pl)     segdescr_usual_init(theGDT[index], type, 0xFFFFF, 0, pl, SD_GRAN_4Kb)

void gdt_setup(void) {
    memset(theGDT, 0, N_GDT * sizeof(struct segdescr));
    /*
    segdescr_init(theGDT + SEL_KERN_CS.index, CODE_SEGDESC, 0xFFFFF, 0x0, SEGDESC_TYPE_ER_CODE, PL_KERN, SEGDESC_4KB_GRANULARITY);
    segdescr_init(theGDT + SEL_KERN_DS.index, DATA_SEGDESC, 0xFFFFF, 0x0, SEGDESC_TYPE_RW_DATA, PL_KERN, SEGDESC_4KB_GRANULARITY);
    segdescr_init(theGDT + SEL_USER_CS.index, CODE_SEGDESC, 0xFFFFF, 0x0, SEGDESC_TYPE_ER_CODE, PL_USER, SEGDESC_4KB_GRANULARITY);
    segdescr_init(theGDT + SEL_USER_DS.index, DATA_SEGDESC, 0xFFFFF, 0x0, SEGDESC_TYPE_RW_DATA, PL_USER, SEGDESC_4KB_GRANULARITY);
    //segdescr_init(theGDT + (DEFAULT_LDT >> 3), SYS_SEGDESC, 0xFFFFF, 0x0, SEGDESCR_TYPE_RW_DATA, PL_USER, SEGDESC_4KB_GRANULARITY);
    */
    gdt_entry_init((SEL_KERN_CS >> 3), SD_TYPE_ER_CODE, PL_KERN);
    gdt_entry_init((SEL_KERN_DS >> 3), SD_TYPE_RW_DATA, PL_KERN);
    gdt_entry_init((SEL_USER_CS >> 3), SD_TYPE_ER_CODE, PL_USER);
    gdt_entry_init((SEL_USER_DS >> 3), SD_TYPE_RW_DATA, PL_USER);

    print_mem(theGDT, 0x100);
	gdt_load(N_GDT, theGDT);
}

void gdt_info(void) {
    k_printf("\nGDT is at 0x%x\n", (uint)theGDT);
}


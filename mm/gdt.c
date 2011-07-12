#include <dev/cpu.h>
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

#define gdt_entry_init(index, type, pl)     segdescr_usual_init(theGDT[index], type, 0xFFFFF, 0, pl, SD_GRAN_4Kb)

void gdt_setup(void) {
    memset(theGDT, 0, N_GDT * sizeof(struct segdescr));

    gdt_entry_init((SEL_KERN_CS >> 3), SD_TYPE_ER_CODE, PL_KERN);
    gdt_entry_init((SEL_KERN_DS >> 3), SD_TYPE_RW_DATA, PL_KERN);
    gdt_entry_init((SEL_USER_CS >> 3), SD_TYPE_ER_CODE, PL_USER);
    gdt_entry_init((SEL_USER_DS >> 3), SD_TYPE_RW_DATA, PL_USER);

	gdt_load(N_GDT, theGDT);
}

void gdt_info(void) {
    k_printf("\nGDT is at 0x%x\n", (uint)theGDT);
}


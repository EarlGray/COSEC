#include <dev/memdev.h>

#include <log.h>

static void ramfs_init(void);

chrdriver_t mem_chr_drv = {
    .drv = {
        .dev_family = CHR_MEMDEVS,
        .name = "mem",
    },
    .init = null,
};

blkdriver_t ram_blk_drv = {
    .drv = {
        .dev_family = BLK_RAM,
        .name = "ram",
    },
    .init = ramfs_init,
};

static void ramfs_init(void) {
    k_printf("Creating devices for modules");
}

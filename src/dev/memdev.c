#include <dev/memdev.h>

#include <log.h>

static void ramfs_init(void);

driver_t mem_drv = {
    .dev_type = DEV_CHR,
    .dev_class = CHR_MEMDEVS,
    .name = "mem",

    .init_devclass = null,
};

driver_t ram_drv = {
    .dev_type = DEV_BLK,
    .dev_class = BLK_RAM,
    .name = "ram",

    .init_devclass = ramfs_init,
};

static void ramfs_init(void) {
    k_printf("TODO: Creating devices for modules...\n");
}

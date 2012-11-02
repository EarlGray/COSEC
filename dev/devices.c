#include <dev/table.h>
#include <dev/block.h>

#include <log.h>

chrdriver_t* theChrDeviceTable[N_CHR] = { 0 };
blkdriver_t* theBlkDeviceTable[N_BLK] = { 0 };

int register_chrdriver(chrdriver_t* chrdrv, index_t family);
int register_blkdriver(blkdriver_t* blkdrv, index_t family);

int unregister_chrdriver(index_t family);
int unregister_blkdriver(index_t family);

static inline void init_driver(driver_t *drv) {
    if (drv->init)
        drv->init();
}

int register_chrdriver(chrdriver_t *chrdrv, index_t family) {
    assertf(! theChrDeviceTable[family], "Family %d is already registered", family);

    theChrDeviceTable[family] = chrdrv;
    init_driver(chrdrv);
}

int register_driver(driver_t *chrdrv, dev_t type, index_t family) {
    assertf(! theChrDeviceTable[family], "Family %d is already registered", family);

    theChrDeviceTable[family] = chrdrv;

    init_driver(
}

#include <dev/memdev.h>

void dev_setup(void) {
    register_chrdriver(&mem_chr_drv, CHR_MEMDEVS);   

    register_blkdriver(&ram_blk_drv, BLK_RAM);
}

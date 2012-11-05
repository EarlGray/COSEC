#include <dev/devices.h>
#include <dev/block.h>

#include <log.h>

driver_t* theChrDeviceTable[N_CHR] = { 0 };
driver_t* theBlkDeviceTable[N_BLK] = { 0 };

int register_driver(driver_t* drv);

int unregister_driver(index_t family);

static inline void init_driver(driver_t *drv) {
    if (drv->init_devclass) drv->init_devclass();
}

int register_driver(driver_t *drv) {
    init_driver(drv);
}

#include <dev/memdev.h>

void dev_setup(void) {
    register_driver(&mem_drv);   

    register_driver(&ram_drv);
}

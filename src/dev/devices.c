#include <stdlib.h>

#include <dev/devices.h>
#include <log.h>

/*
 *   Unspecified (0) character devices family
 */

static device_t *chr0devclass_get_device(mindev_t mindev) {
    UNUSED(mindev);
    return NULL;
}

devclass_t chr0_device_family = {
    .dev_type       = DEV_CHR,
    .dev_maj        = 0,
    .dev_class_name = "virtual character devices",

    .get_device     = chr0devclass_get_device,
    .init_devclass  = NULL,
};


/*
 *  Device tables bookkeeping
 */

devclass_t* theChrDeviceTable[N_CHR] = { 0 };
devclass_t* theBlkDeviceTable[N_BLK] = { 0 };

void devclass_register(devclass_t *devclass) {
    assertv(devclass, "devclass_register(NULL)");

    switch (devclass->dev_type) {
        case DEV_CHR: theChrDeviceTable[ devclass->dev_maj ] = devclass; break;
        case DEV_BLK: theBlkDeviceTable[ devclass->dev_maj ] = devclass; break;
        default: logmsgef("devclass_register(): unknown device type"); return;
    }

    if (devclass->init_devclass)
        devclass->init_devclass();
}


void dev_setup(void) {
    devclass_register(&chr0_device_family);
}

#include <stdlib.h>

#include <mem/pmem.h>

#include <fs/devices.h>
#include <log.h>

/*
 *   Unspecified (0) character devices family
 */

static device *chr0devclass_get_device(mindev_t mindev) {
    UNUSED(mindev);
    return NULL;
}

devclass  chr0_device_family = {
    .dev_type       = DEV_CHR,
    .dev_maj        = CHR_VIRT,
    .dev_class_name = "virtual character devices",

    .get_device     = chr0devclass_get_device,
    .init_devclass  = NULL,
};


/*
 *  RAM character devices
 */


static device * get_ram_char_device(mindev_t num) {
    UNUSED(num);
    return NULL;
}

devclass  chr1_device_family = {
    .dev_type       = DEV_CHR,
    .dev_maj        = CHR_MEMDEV,
    .dev_class_name = "character memory devices",

    .get_device     = get_ram_char_device,
    .init_devclass  = NULL,
};

/*
 *  RAM block devices
 */

static device * get_ram_block_device(mindev_t mindev) {
    UNUSED(mindev);
    return NULL;
}

devclass  blk1_device_family = {
    .dev_type       = DEV_BLK,
    .dev_maj        = BLK_RAM,
    .dev_class_name = "ram block devices",

    .get_device     = get_ram_block_device,
    .init_devclass  = NULL,
};


/*
 *  Device tables bookkeeping
 */

devclass * theChrDeviceTable[N_CHR] = { 0 };
devclass * theBlkDeviceTable[N_BLK] = { 0 };

void devclass_register(devclass *dclss) {
    assertv(dclss, "devclass_register(NULL)");

    switch (dclss->dev_type) {
        case DEV_CHR: theChrDeviceTable[ dclss->dev_maj ] = dclss; break;
        case DEV_BLK: theBlkDeviceTable[ dclss->dev_maj ] = dclss; break;
        default: logmsgef("devclass_register(): unknown device type"); return;
    }

    if (dclss->init_devclass)
        dclss->init_devclass();
}

device * device_by_devno(devicetype_e  ty, dev_t devno) {
    majdev_t maj = gnu_dev_major(devno);
    mindev_t min = gnu_dev_minor(devno);

    devclass **table = NULL;
    if (ty == DEV_CHR) {
        if (maj >= N_CHR) return NULL;
        table = theChrDeviceTable;
    }
    if (ty == DEV_BLK) {
        if (maj >= N_BLK) return NULL;
        table = theBlkDeviceTable;
    }

    devclass *dclss = table[ maj ];
    if (!dclss) return NULL;

    if (!dclss->get_device) return NULL;
    return dclss->get_device(min);
}


void dev_setup(void) {
    devclass_register(&chr0_device_family);
}

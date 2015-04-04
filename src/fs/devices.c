#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>

#include <cosec/log.h>

#include <mem/pmem.h>

#include <dev/screen.h>
#include <dev/tty.h>

#include <fs/devices.h>

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
 *  Generic device operations
 */

int bdev_blocking_read(
        device *dev, off_t pos, char *buf, size_t buflen, size_t *written)
{
    const char *funcname = __FUNCTION__;

    int ret = 0;
    size_t i;
    size_t bytes_done = 0;

    if (!dev->dev_ops->dev_size_of_block)  { ret = ENOSYS; goto error_exit; }
    if (!dev->dev_ops->dev_size_in_blocks) { ret = ENOSYS; goto error_exit; }
    if (!dev->dev_ops->dev_get_roblock)    { ret = ENOSYS; goto error_exit; }

    size_t blksz = dev->dev_ops->dev_size_of_block(dev);
    off_t maxblock = dev->dev_ops->dev_size_in_blocks(dev);

    size_t startoffset = pos % blksz;
    size_t startbytes = (startoffset ? blksz - startoffset : 0);
    size_t nfullblocks = (buflen - startbytes) / blksz;
    size_t finalbytes = (buflen - startbytes) % blksz;

    off_t curblock = pos / blksz;
    const char *blockdata = NULL;

    if (startbytes) {
        if (curblock >= maxblock) { ret = ENXIO; goto error_exit; }

        blockdata = dev->dev_ops->dev_get_roblock(dev, curblock);
        if (!curblock) { ret = ENXIO; goto error_exit; }

        memcpy(buf, blockdata + startoffset, startbytes);

        if (dev->dev_ops->dev_forget_block)
            dev->dev_ops->dev_forget_block(dev, curblock);
        bytes_done += startbytes;
        buf += startbytes;
        ++curblock;
    }

    for (i = 0; i < nfullblocks; ++i) {
        if (curblock >= maxblock) { ret = ENXIO; goto error_exit; }

        blockdata = dev->dev_ops->dev_get_roblock(dev, curblock);
        if (!curblock) { ret = ENXIO; goto error_exit; }

        memcpy(buf, blockdata, blksz);

        if (dev->dev_ops->dev_forget_block)
            dev->dev_ops->dev_forget_block(dev, curblock);
        bytes_done += blksz;
        buf += blksz;
        ++curblock;
    }

    if (finalbytes) {
        if (curblock >= maxblock) { ret = ENXIO; goto error_exit; }

        blockdata = dev->dev_ops->dev_get_roblock(dev, curblock);
        if (!curblock) { ret = ENXIO; goto error_exit; }

        memcpy(buf, blockdata, finalbytes);

        if (dev->dev_ops->dev_forget_block)
            dev->dev_ops->dev_forget_block(dev, curblock);
        bytes_done += finalbytes;
        buf += finalbytes;
    }

error_exit:
    logmsgdf("%s: ret=%d, bytes_done=%d\n", ret, bytes_done);
    if (written) *written = bytes_done;
    return ret;
}


int bdev_blocking_write(
        struct device *dev, off_t pos,
        const char *buf, size_t buflen, size_t *written)
{
    const char *funcname = __FUNCTION__;

    int ret = 0;
    size_t i;
    size_t bytes_done = 0;

    if (!dev->dev_ops->dev_size_of_block)  { ret = ENOSYS; goto error_exit; }
    if (!dev->dev_ops->dev_size_in_blocks) { ret = ENOSYS; goto error_exit; }
    if (!dev->dev_ops->dev_get_roblock)    { ret = ENOSYS; goto error_exit; }

    size_t blksz = dev->dev_ops->dev_size_of_block(dev);
    off_t maxblock = dev->dev_ops->dev_size_in_blocks(dev);

    size_t startoffset = pos % blksz;
    size_t startbytes = (startoffset ? blksz - startoffset : 0);
    size_t nfullblocks = (buflen - startbytes) / blksz;
    size_t finalbytes = (buflen - startbytes) % blksz;

    off_t curblock = pos / blksz;
    char *blockdata = NULL;

    if (startbytes) {
        if (curblock >= maxblock) { ret = ENXIO; goto error_exit; }

        blockdata = dev->dev_ops->dev_get_rwblock(dev, curblock);
        if (!curblock) { ret = ENXIO; goto error_exit; }

        memcpy(blockdata + startoffset, buf, startbytes);

        if (dev->dev_ops->dev_forget_block)
            dev->dev_ops->dev_forget_block(dev, curblock);
        bytes_done += startbytes;
        buf += startbytes;
        ++curblock;
    }

    for (i = 0; i < nfullblocks; ++i) {
        if (curblock >= maxblock) { ret = ENXIO; goto error_exit; }

        blockdata = dev->dev_ops->dev_get_rwblock(dev, curblock);
        if (!curblock) { ret = ENXIO; goto error_exit; }

        memcpy(blockdata, buf, blksz);

        if (dev->dev_ops->dev_forget_block)
            dev->dev_ops->dev_forget_block(dev, curblock);
        bytes_done += blksz;
        buf += blksz;
        ++curblock;
    }

    if (finalbytes) {
        if (curblock >= maxblock) { ret = ENXIO; goto error_exit; }

        blockdata = dev->dev_ops->dev_get_rwblock(dev, curblock);
        if (!curblock) { ret = ENXIO; goto error_exit; }

        memcpy(blockdata, buf, finalbytes);

        if (dev->dev_ops->dev_forget_block)
            dev->dev_ops->dev_forget_block(dev, curblock);
        bytes_done += finalbytes;
        buf += finalbytes;
    }

error_exit:
    logmsgdf("%s: ret=%d, bytes_done=%d\n", ret, bytes_done);
    if (written) *written = bytes_done;
    return ret;
}

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
    /* character devices */
    devclass_register( &chr0_device_family );
    devclass_register( &chr1_device_family );

    devclass_register( get_vcs_devclass() );
    devclass_register( get_tty_devclass() );

    /* block devices */
}

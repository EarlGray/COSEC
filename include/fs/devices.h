#ifndef __DRIVERS_TABLE_H__
#define __DRIVERS_TABLE_H__

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

#define N_CHR           30
#define N_BLK           12

enum devicetype_e {
    DEV_CHR = 0,
    DEV_BLK = 1,
};

/* major numbers */
enum char_device_family {
    CHR_VIRT        = 0,
    CHR_MEMDEV      = 1,
    CHR_PTY_MASTER  = 2,
    CHR_PTY_SLAVE   = 3,
    CHR_TTY         = 4,
    CHR_TTY_OTH     = 5,
    CHR_LP          = 6,
    CHR_VCS         = 7,
    CHR_SCSI_TAPE   = 9,
    CHR_MISC        = 10,
    CHR_KBD         = 11,
    CHR_FRAMEBUF    = 29,
};

/* chrdev maj=0 */
enum char_virtual_devices {
    CHR0_UNSPECIFIED = 0,
    CHR0_SYSFS       = 1,
};

/* chrdev maj=1 */
enum chrmem_device {
    CHRMEM_MEM      = 1,
    CHRMEM_NULL     = 3,
    CHRMEM_PORT     = 4,
    CHRMEM_ZERO     = 5,
    CHRMEM_FULL     = 7,
    CHRMEM_RAND     = 8,
    CHRMEM_URAND    = 9,
    CHRMEM_KMSG     = 11,
};


/* major numbers */
enum block_device_family {
    BLK_VIRT        = 0,
    BLK_RAM         = 1,
    BLK_FLOPPY      = 2,
    BLK_IDE         = 3,
    BLK_LOOPBACK    = 7,
    BLK_SCSI_DISK   = 8,
    BLK_RAID        = 9,
    BLK_SCSI_CDROM  = 11,
};

typedef enum devicetype_e   devicetype_e;
typedef index_t     majdev_t, mindev_t;

typedef struct devclass  devclass;
typedef struct device    device;

struct devclass {
    devicetype_e  dev_type;
    majdev_t      dev_maj;
    const char   *dev_class_name;

    device  *(*get_device)(mindev_t num);
    void     (*init_devclass)(void);
};


struct device_operations {
    /**
     * \brief  gets a cached block from device in read-only mode
     */
    const char *(*dev_get_roblock)(device *dev, off_t block);

    /**
     * \brief  get a cached block from device in read-write mode
     *         the block will be dirty after `dev_forget_block()`.
     */
    char *      (*dev_get_rwblock)(device *dev, off_t block);

    /**
     * \brief  tells a device that the gotten block will not be used anymore.
     */
    int         (*dev_forget_block)(device *dev, off_t block);

    /** \brief returns block size in bytes */
    size_t      (*dev_size_of_block)(device *dev);

    /** \brief returns (block) device size in blocks */
    off_t       (*dev_size_in_blocks)(device *dev);

    /* mostly character devices operations */
    /**
     * \brief   non-blocing read from (mostly character) device
     * @param written   *written contains number of actually written bytes if not NULL;
     * @param pos       read offset for devices that support offset;
     * @return  0 if data available;
     *          EAGAIN if no data pending;
     *          other error if device is in broken state;
     */
    int     (*dev_read_buf)(device *dev, char *buf, size_t buflen, size_t *written, off_t pos);

    /**
     * \brief
     */
    int     (*dev_write_buf)(device *dev, const char *buf, size_t buflen, size_t *written, off_t pos);

    /**
     * \brief  returns true if there are data pending for a character device
     *         If it's NULL, it usually means that the device may be read anytime.
     */
    bool    (*dev_has_data)(device *dev);

    int     (*dev_ioctlv)(device *dev, int ioctl_code, va_list ap);
};

struct device {
    devicetype_e    dev_type;   // character or block
    majdev_t        dev_clss;   // generic driver
    mindev_t        dev_no;     // device index in the family

    struct device_operations  *dev_ops;  // yep, devopses should care about devices
};

/**
 * \brief  blocking read from a block device at `pos`
 */
int bdev_blocking_read(dev_t devno, off_t pos, char *buf, size_t buflen, size_t *written);
int cdev_blocking_read(dev_t devno, off_t pos, char *buf, size_t buflen, size_t *written);

/**
 * \brief  get device structure
 */
device * device_by_devno(devicetype_e  ty, dev_t devno);


void dev_setup(void);

#endif //__DRIVERS_TABLE_H__

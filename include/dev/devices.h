#ifndef __DRIVERS_TABLE_H__
#define __DRIVERS_TABLE_H__

#include <stdint.h>

#define N_CHR           30
#define N_BLK           12

enum devicetype_e {
    DEV_CHR = 0,
    DEV_BLK = 1,
};

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

enum char_virtual_devices {
    CHR_UNSPECIFIED = 0,
    CHR_SYSFS       = 1,
};

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

typedef enum devicetype_e devicetype_e;
typedef index_t majdev_t, mindev_t;

typedef struct devclass_t  devclass_t;
typedef struct device_t    device_t;

struct devclass_t {
    devicetype_e  dev_type;
    majdev_t      dev_maj;
    const char   *dev_class_name;

    device_t *(*get_device)(mindev_t num);
    void (*init_devclass)(void);
};

struct device_t {
    devicetype_e dev_type;  // character or block
    majdev_t dev_clss;      // generic driver
    mindev_t dev_no;        // device index in the family
    const char devfs_name;  // how it should appear in /dev

    union {
        struct chardevice_t {
        } as_chardev;

        struct blockdevice_t {
        } as_blockdev;
    };
};

void dev_setup(void);

#endif //__DRIVERS_TABLE_H__

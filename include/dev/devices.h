#ifndef __DRIVERS_TABLE_H__
#define __DRIVERS_TABLE_H__

#include <stdint.h>

#define N_CHR           30
#define N_BLK           12
#define N_NET           0

#define CHR_MEMDEVS     1
#define CHR_PTY_MASTERS 2
#define CHR_PTY_SLAVES  3
#define CHR_TTYS        4
#define CHR_TTY_OTH     5
#define CHR_LP          6
#define CHR_VCS         7
#define CHR_SCSI_TAPE   9
#define CHR_MISC        10
#define CHR_KBD         11
#define CHR_FRAMEBUF    29

#define BLK_RAM         1
#define BLK_FLOPPY      2
#define BLK_IDE         3
#define BLK_LOOPBACK    7
#define BLK_SCSI_DISK   8
#define BLK_RAID        9
#define BLK_SCSI_CDROM  11

typedef enum devicetype_e devicetype_e;
enum devicetype_e {
    DEV_CHR = 0,
    DEV_BLK = 1,
    DEV_NET = 2,
};

typedef index_t majdev_t, mindev_t;

typedef struct cache cache_t;

typedef struct driver_t driver_t;
typedef struct device_t device_t;
typedef struct chrdriver_t chrdriver_t;
typedef struct blkdriver_t blkdriver_t;

struct driver_t {
    /* who am I */
    devicetype_e dev_type;
    majdev_t dev_class;
    const char *name;

    void (*init_devclass)(void);
    void (*deport_devclass)(void);
};

struct device_t {
    mindev_t dev_no;        // device index in the family
    driver_t* driver;       // generic driver
    cache_t *cache;         // null if no caching for device
    uint blksz;             // 1 for character devices
};

void dev_setup(void);

#endif //__DRIVERS_TABLE_H__

#ifndef __DRIVERS_TABLE_H__
#define __DRIVERS_TABLE_H__

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

#include <std/sys/types.h>

typedef uint majdev_t, mindev_t;

typedef struct cache cache_t;

typedef struct driver_t driver_t;
typedef struct chrdriver_t chrdriver_t;
typedef struct blkdriver_t blkdriver_t;

struct driver_t {
    majdev_t dev_family;
    const char *name;
};

struct blkdriver_t {
    driver_t drv;
};

struct chrdriver_t {
    driver_t drv;
    
    void (*init)(void);
};

typedef struct block_dev_t {
    mindev_t dev_no;
    blkdriver_t* driver;
    cache_t *cache;
    uint blksz;
} block_dev_t;

#endif //__DRIVERS_TABLE_H__

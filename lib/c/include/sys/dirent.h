#ifndef __CSCLIBC_DIRENT_H__
#define __CSCLIBC_DIRENT_H__

#include <limits.h>
#include <stdint.h>

enum dentry_type {
    DT_UNKNOWN  = 0,
    DT_FIFO     = 1,
    DT_CHR      = 2,
    DT_DIR      = 4,
    DT_BLK      = 6,
    DT_REG      = 8,
    DT_LNK      = 10,
    DT_SOCK     = 12,
    DT_WHT      = 14,
};

struct dirent {
    inode_t     d_ino;
    uint16_t    d_reclen;
    uint8_t     d_type;
    uint8_t     d_namlen;
    char        d_name[UCHAR_MAX];
};

#endif //__CSCLIBC_DIRENT_H__

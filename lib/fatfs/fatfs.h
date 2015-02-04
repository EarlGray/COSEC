#ifndef __COSEC_FATFS_H__
#define __COSEC_FATFS_H__

#include <stdint.h>

#ifndef __packed
# define __packed __attribute__((packed))
#endif

struct common_bios_param_block __packed {
    uint16_t bpb_bytes_per_sector;
    uint8_t  bpb_sector_per_cluster;
    uint16_t bpb_reserved_sectors_per_cluster;
    uint8_t  bpb_n_fats;
    uint16_t bpb_n_sectors;
    uint8_t  bpb_media_type;
    uint16_t bpb_n_fat_sectors;
};

#endif // __COSEC_FATFS_H__

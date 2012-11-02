#include <dev/memdev.h>

#include <log.h>

chrdriver_t mem_chr_drv = {
    .drv = {
        .dev_family = CHR_MEMDEVS,
        .name = "mem",
    },
    .init = null,
};

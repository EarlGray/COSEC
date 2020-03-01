
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include <cosec/log.h>

#include "dev/serial.h"

#include "mem/kheap.h"
#include "fs/devices.h"


#define LOGBUF_SIZE  4096

#define COM_LOGGING  (1)

static int kmsg_writebuf(
    device *dev, const char *buf, size_t buflen,
    size_t *written, off_t pos
);

struct device_operations kmsg_ops = {
    .dev_get_roblock = NULL,
    .dev_get_rwblock = NULL,
    .dev_forget_block = NULL,
    .dev_size_of_block = NULL,

    .dev_size_in_blocks = NULL,
    .dev_read_buf = NULL,
    .dev_write_buf = kmsg_writebuf,
    .dev_has_data = NULL,
    .dev_ioctlv = NULL,
};

struct device kmsg_dev = {
    .dev_type = DEV_CHR,
    .dev_clss = CHR_MEMDEV,
    .dev_no   = CHRMEM_KMSG,
    .dev_data = NULL,

    .dev_ops  = &kmsg_ops
};

device * kmsg_device_get(void) {
    return &kmsg_dev;
}

int logging_setup() {
#if COM_LOGGING
    serial_configure(COM1_PORT, 1);
    k_printf("COM1_PORT configured for logging...\n");
    serial_puts(COM1_PORT, "\n\tWelcome to COSEC\n\n");
#endif
    return 0;
}

static char logbuf[LOGBUF_SIZE];


static int kmsg_writebuf(
    device *dev, const char *buf, size_t buflen,
    size_t *written, off_t pos)
{
    logmsgdf("kmsg_writebuf(pos=%d)\n", pos);
#if COM_LOGGING
    serial_puts(COM1_PORT, buf);
#else
    k_printf("# %s", buf);
#endif
    if (written) *written = buflen;
    return 0;
}

int vlprintf(const char *fmt, va_list ap) {
    int ret = 0;
    int i;

    ret = vsnprintf(logbuf, LOGBUF_SIZE, fmt, ap);
    if (ret >= (int)LOGBUF_SIZE)
        logbuf[LOGBUF_SIZE - 1] = 0;

#if COM_LOGGING
    serial_puts(COM1_PORT, logbuf);
#else
    k_printf("# %s", logbuf);
#endif

    return ret;
}

int lprintf(const char *fmt, ...) {
    int ret;
    va_list ap;
    va_start(ap, fmt);
    ret = vlprintf(fmt, ap);
    va_end(ap);
    return ret;
}


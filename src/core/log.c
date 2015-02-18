#include <log.h>
#include <mem/kheap.h>
#include <dev/serial.h>

#include <stdio.h>
#include <stdarg.h>

#define LOGBUF_SIZE  4096

int logging_setup() {
    serial_configure(COM1_PORT, 1);
    k_printf("COM1_PORT configured for logging...\n");
    return 0;
}

static logbuf[LOGBUF_SIZE];

int vlprintf(const char *fmt, va_list ap) {
    int ret = 0;
    int i;

    ret = vsnprintf(logbuf, LOGBUF_SIZE, fmt, ap);
    if (ret >= (int)LOGBUF_SIZE)
        logbuf[LOGBUF_SIZE - 1] = 0;

#if 1
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


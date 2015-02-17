
#include <log.h>
#include <dev/serial.h>

#include <stdarg.h>

int logging_setup() {
    k_printf("COM1_PORT configured for logging...\n");
    serial_configure(COM1_PORT, 1);
}

int vlprintf(const char *fmt, va_list ap) {
    /* TODO */   
}

int lprintf(const char *fmt, ...) {
    int ret;
    va_list ap;
    va_start(ap, fmt);   
    ret = vlprintf(fmt, ap);
    va_end(ap);
    return ret;
}


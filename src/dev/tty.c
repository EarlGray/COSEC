#include <stdlib.h>
#include <termios.h>

#include <dev/kbd.h>
#include <dev/screen.h>

#include <fs/devices.h>
#include <log.h>

struct tty_device {
    struct device  dev;

    struct termios tc;

    mindev_t vcs;
};


mindev_t theActiveTTY;


static int tty_read() {
    return ETODO;
}

static int tty_write() {
    return ETODO;
}

static bool tty_has_data() {
    return false;
}

static int tty_ioctl() {
    return ETODO;
}

struct device_operations  tty_ops = {
    /* not a block device */
    .dev_get_roblock = NULL,
    .dev_get_rwblock = NULL,
    .dev_forget_block = NULL,
    .dev_size_of_block = NULL,
    .dev_size_in_blocks = NULL,

    .dev_read_buf = tty_read,
    .dev_write_buf = tty_write,
    .dev_has_data = tty_has_data,

    .dev_ioctlv = tty_ioctl,
};


static device * get_tty_device(mindev_t devno) {
    return NULL;
}


static void init_tty_family(void) {
    const char *funcname = "init_tty_family";
    logmsgf("%s()\n", funcname);

    theActiveTTY = 0;
}

struct devclass tty_family = {
    .dev_type = DEV_CHR,
    .dev_maj = CHR_TTY,
    .dev_class_name = "tty devices",
    .get_device = get_tty_device,
    .init_devclass = init_tty_family,
};

struct devclass * get_tty_devclass(void) {
    return &tty_family;
}

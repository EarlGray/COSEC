#include <stdlib.h>
#include <termios.h>

#include <dev/kbd.h>
#include <dev/screen.h>

#include <fs/devices.h>
#include <log.h>

typedef  struct tty_device       tty_device;
typedef  struct tty_input_queue  tty_inpqueue;

struct tty_input_queue {
    /* circular buffer */
    unsigned start;
    unsigned end;

    uint8_t buf[MAX_INPUT];
};

struct tty_device {
    struct device           tty_dev;

    volatile tty_inpqueue   tty_inpq;
    struct termios          tty_conf;
    struct winsize          tty_size;

    mindev_t                tty_vcs;
};

const struct termios
stty_sane = {
    .c_iflag = ICRNL | IXON | IXANY | IMAXBEL,
    .c_oflag = OPOST | ONLCR,
    .c_cflag = CREAD | CS8 | HUPCL,
    .c_lflag = ICANON | ISIG | IEXTEN | ECHO | ECHOE | ECHOKE | ECHOCTL | PENDIN,
    .c_cc = {
        [VEOF]  = 0x00,
        //[VEOL]  = 0x ,  /* ^J */
        [VINTR] = 0x03, /* ^C */
        //[VQUIT] = ,
     },
    .c_ispeed = B38400,
    .c_ospeed = B38400,
};

/*
struct termios stty_raw = {
};
*/


/* global state */
mindev_t    theActiveTTY;
tty_device  theVcsTTY[N_VCSA_DEVICES];

tty_device* theTTYlist[MAX_TTY] = { 0 };


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
    return_dbg_if(!(devno < N_VCSA_DEVICES), NULL, __FUNCTION__": ENOENT");

    return &(theTTYlist[devno]->tty_dev);
}


static void init_tty_family(void) {
    const char *funcname = "init_tty_family";
    logmsgf("%s()\n", funcname);
    int i;

    theActiveTTY = 0;
    for (i = 0; i < N_VCSA_DEVICES; ++i) {
        tty_device *tty = theVcsTTY + i;
        tty->tty_vcs = i;

        device *dev = &tty->tty_dev;
        dev->dev_type = DEV_CHR;
        dev->dev_clss = CHR_TTY;
        dev->dev_no   = i;
        dev->dev_ops  = &tty_ops;

        tty->tty_conf = stty_sane;
        tty->tty_size.wx = SCR_WIDTH;
        tty->tty_size.wy = SCR_HEIGHT;

        tty->tty_inpq.start = tty->tty_inpq.end = 0;
    }
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

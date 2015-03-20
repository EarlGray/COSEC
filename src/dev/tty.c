#include <stdlib.h>
#include <ctype.h>
#include <termios.h>

#include <dev/kbd.h>
#include <dev/screen.h>

#include <fs/devices.h>
#include <dev/tty.h>

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


static device * get_tty_device(mindev_t devno) {
    return_dbg_if(!(devno < N_VCSA_DEVICES), NULL, __FUNCTION__": ENOENT");

    return &(theTTYlist[devno]->tty_dev);
}


static int dev_tty_read() {
    return ETODO;
}


static inline void tty_linefeed(int vcsno) {
    vcsa_newline(vcsno);
}

static int dev_tty_write(device  *dev, const char *buf, size_t buflen, size_t *written, off_t pos) {
    UNUSED(pos);
    const char *funcname = __FUNCTION__;

    struct tty_device *ttydev = dev->dev_data;
    return_log_if(!ttydev, EKERN, "%s: no ttydev", funcname);

    mindev_t           ttyvcs = ttydev->tty_vcs;
    return_log_if(ttyvcs >= N_VCSA_DEVICES, ETODO,
            "%s: dev_vcs=%d\n", funcname, ttydev->tty_vcs);

    const char *s = buf;
    while ((size_t)(s - buf) < buflen) {
        if (iscntrl(*s)) {
            switch (*s) {
              case BEL: /* \a, 0x07, ^G  -- ignored for now */ break;
              case BS: /* BS: \b, 0x08, ^H  -- erase a character */
                vcsa_set_char(ttyvcs, ' '); vcsa_move_cursor_back(ttyvcs);
                break;
              case TAB: /* HT: \t, 0x09, ^I -- move to a tabstop */
                vcsa_move_cursor_tabstop(ttyvcs);
                break;
              case LF: /* 0x0a, ^J */
                tty_linefeed(ttyvcs);
                break;
              case VT: /* 0x0b, ^K */
              case FF: /* 0x0c, ^L */
                vcsa_move_cursor_by(ttyvcs, 0, 1);
                break;
              case CR: /* 0x0d, ^M */
                vcsa_set_cursor(ttyvcs, 0, VCS_DO_NOT_MOVE);
                break;
              case DEL: /* ignore */ break;
              case ESC: /* 0x1b, ^[ -- start an escape sequence */
                ++s;
                switch (*s) {
                  case 'c': /* RIS, reset */ vcsa_clear(ttyvcs); break;
                  case 'D': /* IND */ vcsa_move_cursor_by(ttyvcs, 0, 1); break;
                  case 'E': /* NEL */ vcsa_newline(ttyvcs); break;
                  case 'H': /* HTS */ logmsgef("%s: TODO: set tabstop at current column", funcname); break;
                  case 'M': /* RI  */ vcsa_move_cursor_by(ttyvcs, 0, -1); break; /* reverse linefeed */
                  case '[': /* CSI -- Control Sequence Introducer */
                    ++s;
                    /* TODO: read params */
                    /* TODO: handle CSIs */
                    break;
                  default: logmsgf("%s: unknown sequence ESC \\x%x\n", funcname, (uint)*s); break;
                }
                break;
              default:
                vcsa_cprint(ttyvcs, '^'); vcsa_cprint(ttyvcs, '@' + *s);
            }
        } else {
            vcsa_cprint(ttyvcs, *s);
        }
        ++s;
    }

    if (written) *written = buflen;
    return 0;
}

int tty_write(mindev_t ttyno, const char *buf, size_t buflen) {
    const char *funcname = __FUNCTION__;

    device *dev = get_tty_device(ttyno);
    return_dbg_if(!dev, ENOENT,
            "%s: ttyno=%d\n", funcname, ttyno);

    return dev_tty_write(dev, buf, buflen, NULL, 0);
}

static bool dev_tty_has_data() {
    return false;
}

static int dev_tty_ioctl() {
    return ETODO;
}

struct device_operations  tty_ops = {
    /* not a block device */
    .dev_get_roblock    = NULL,
    .dev_get_rwblock    = NULL,
    .dev_forget_block   = NULL,
    .dev_size_of_block  = NULL,
    .dev_size_in_blocks = NULL,

    .dev_read_buf   = dev_tty_read,
    .dev_write_buf  = dev_tty_write,
    .dev_has_data   = dev_tty_has_data,

    .dev_ioctlv     = dev_tty_ioctl,
};



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
        dev->dev_data = (void *)tty;
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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>

#include <dev/kbd.h>
#include <dev/screen.h>

#include <fs/devices.h>
#include <dev/tty.h>
#include <arch/i386.h>

#define __DEBUG
#include <log.h>

typedef  struct tty_device       tty_device;
typedef  struct tty_input_queue  tty_inpqueue;

struct tty_input_queue {
    /* circular buffer */
    size_t start;
    size_t end;

    uint8_t buf[MAX_INPUT];
};


struct tty_device {
    struct device           tty_dev;

    enum tty_kbdmode        tty_kbmode;
    volatile tty_inpqueue   tty_inpq;
    struct termios          tty_conf;
    struct winsize          tty_size;

    mindev_t                tty_vcs;
};


/* global state */
mindev_t    theActiveTTY;
tty_device  theVcsTTY[N_VCSA_DEVICES];

tty_device* theTTYlist[MAX_TTY] = { 0 };


const struct termios
stty_sane = {
    .c_iflag = ICRNL | IXON | IXANY | IMAXBEL,
    .c_oflag = OPOST | ONLCR,
    .c_cflag = CREAD | CS8 | HUPCL,
    .c_lflag = ICANON | ISIG | IEXTEN | ECHO | ECHOE | ECHOKE | ECHOCTL | PENDIN,
    .c_cc = {
        [VEOF]  = 0x00,
        [VINTR] = 0x03, /* ^C */
     },
    .c_ispeed = B38400,
    .c_ospeed = B38400,
};

/*
struct termios stty_raw = {
};
*/


/*
 *      TTY input queue
 */

static inline void small_memcpy(char *dst, const char *src, size_t len) {
    /* Duff's device: is it worth it? */
    switch (len) {
      case 4: dst[3] = src[3];
      case 3: dst[2] = src[2];
      case 2: dst[1] = src[1];
      case 1: dst[0] = src[0];
        break;
      default:
        memcpy(dst, src, len);
    }
}

static size_t tty_inpq_size(tty_inpqueue *inpq) {
    if (inpq->start < inpq->end) {
        return (MAX_INPUT - inpq->start) + inpq->end;
    }

    return inpq->start - inpq->end;
}

static uint8_t tty_inpq_at(tty_inpqueue *inpq, size_t index) {
    if ((inpq->start + index) < MAX_INPUT)
        return *(inpq->buf + inpq->start + index);

    size_t break_offset = index - (MAX_INPUT - inpq->start);
    return *(inpq->buf + break_offset);
}

static int tty_inpq_strchr(volatile tty_inpqueue *inpq, char c) {
    const char *funcname = __FUNCTION__;

    if (inpq->start == inpq->end)
        return -1; /* queue is empty */

    const char *qbuf = (char *)inpq->buf;
    const char *qend = (char *)(inpq->buf + inpq->end);
    char *at;

    if (inpq->start > inpq->end) {
        at = strnchr(qend, inpq->start - inpq->end, c);
        if (!at) return -1;

        return at - (char *)(inpq->buf + inpq->end);
    }

    size_t break_offset = MAX_INPUT - inpq->end;

    at = strnchr(qend, break_offset, c);
    if (at)
        return at - (qbuf + inpq->end);

    at = strnchr(qbuf, inpq->start, c);
    if (at)
        return (at - qbuf) + break_offset;

    return -1;
}

static bool tty_inpq_push(tty_inpqueue *inpq, char buf[], size_t len) {
    size_t start = inpq->start;
    char *qbuf = (char *)inpq->buf;

    if (start < inpq->end) {
        if ((start + len + 1) > inpq->end)
            return false;

        /* TODO: this should be atomic */
        small_memcpy(qbuf + start, buf, len);
        inpq->start += len;
        return true;
    }

    if ((start + len) <= MAX_INPUT) {
        small_memcpy(qbuf + start, buf, len);
        start += len;
        if (start == MAX_INPUT)
            start = 0;
        inpq->start = start;
        return true;
    }

    size_t break_offset = MAX_INPUT - start;
    if ((len - break_offset + 1) > inpq->end)
        return false;
    small_memcpy(qbuf + start, buf, break_offset);
    small_memcpy(qbuf, buf + break_offset, len - break_offset);
    inpq->start = len - break_offset;
    return true;
}

static bool tty_inpq_unpush(tty_inpqueue *inpq, size_t len) {
    for (; len > 0; --len) {
        if (inpq->start == inpq->end)
            return false;

        if (inpq->start == 0)
            inpq->start = MAX_INPUT;

        --inpq->start;
    }
    return true;
}

static size_t tty_inpq_pop(tty_inpqueue *inpq, char *buf, size_t len) {
    size_t end = inpq->end;
    size_t popped = len;

    if (inpq->start == end)
        return 0;

    if (end < inpq->start) {
        if (len > (inpq->start - end))
            popped = (end - inpq->start);

        if (buf) {
            memcpy(buf, inpq->buf + inpq->end, popped);
        }
        inpq->end += popped;
        return popped;
    }

    size_t break_offset = MAX_INPUT - end;
    if (((MAX_INPUT - end) + inpq->start) < len)
        popped = inpq->start + break_offset;

    if (buf) {
        memcpy(buf, inpq->buf + inpq->end, break_offset);
        memcpy(buf + break_offset, inpq->buf, popped - break_offset);
    }
    return popped;
}



static device * get_tty_device(mindev_t devno) {
    const char *funcname = __FUNCTION__;
    return_dbg_if(!(devno < N_VCSA_DEVICES), NULL, "%s: ENOENT", funcname);

    return &(theTTYlist[devno]->tty_dev);
}


static inline void tty_linefeed(int vcsno) {
    vcsa_newline(vcsno);
}

const uint8_t ecma48_to_vga_color[] = {
    [0] = VCSA_ATTR_BLACK,
    [1] = VCSA_ATTR_RED,
    [2] = VCSA_ATTR_GREEN,
    [3] = VCSA_ATTR_ORANGE,
    [4] = VCSA_ATTR_BLUE,
    [5] = VCSA_ATTR_MAGENTA,
    [6] = VCSA_ATTR_CYAN,
    [7] = VCSA_ATTR_GREY,
};

static inline void ecma48_csi(char c, int vcsno, int *a1, int *a2) {
    const char *funcname = __FUNCTION__;
    uint8_t attr;

    int arg1 = (a1 ? *a1 : 1);
    int arg2 = (a2 ? *a2 : 1);

    switch (c) {
      case 'A': /* CUU -- cursor up by arg1 */
        arg1 = -arg1; /* fallthrough */
      case 'B': /* CUD -- cursor down */
        vcsa_move_cursor_by(vcsno, 0, arg1);
        break;
      case 'D': /* CUB -- cursor back */
        arg1 = - arg1; /* fallthrough */
      case 'C': /* CUF -- cursor forward */
        vcsa_move_cursor_by(vcsno, arg1, 0);
        break;
      case 'F': /* CPL -- to prev. lines */
        arg1 = -arg1; /* fallthrough */
      case 'E': /* CNL -- to next lines */
        vcsa_move_cursor_by(vcsno, -SCR_WIDTH, arg1);
        break;
      case 'G': /* CHA -- move to indicated column in the current row */
        vcsa_set_cursor(vcsno, arg1, VCS_DO_NOT_MOVE);
        break;
      case 'H': /* CUP -- set cursor position to (arg2, arg1) */
        vcsa_set_cursor(vcsno, arg2, arg1);
        break;
      case 'J': /* ED -- erase display */
        switch (arg1) {
          case 1:
            if (*a1) /* erase from 1,1 to the cursor */
                vcsa_erase_screen(vcsno, VCSA_ERASE_BEFORE_CURSOR);
            else /* erase from cursor to the end */
                vcsa_erase_screen(vcsno, VCSA_ERASE_AFTER_CURSOR);
            break;
          case 2: /* erase the whole screen */
            vcsa_erase_screen(vcsno, VCSA_ERASE_WHOLE);
            break;
          default:
            logmsgdf("%s: warn: ESC [ %d J\n", funcname, arg1);
        }
        break;
      case 'K': /* EL -- erase line */
        switch (arg1) {
          case 1:
            if (*a1)
                vcsa_erase_line(vcsno, VCSA_ERASE_BEFORE_CURSOR);
            else
                vcsa_erase_line(vcsno, VCSA_ERASE_AFTER_CURSOR);
            break;
          case 2:
            vcsa_erase_line(vcsno, VCSA_ERASE_WHOLE);
            break;
          default:
            logmsgdf("%s: warn: ESC [ %d K\n", funcname, arg1);
        }
        break;

      case 'm':
        if (!a1) arg1 = 0;
        attr = vcsa_get_attribute(vcsno);
        if (0 == arg1) {
            /* reset attributes */
            vcsa_set_attribute(vcsno, VCSA_DEFAULT_ATTRIBUTE);
            break;
        }
        if ((30 <= arg1) && (arg1 <= 37)) {
            /* set foreground color */
            attr &= 0xf0;
            attr |= ecma48_to_vga_color[arg1 - 30];
            vcsa_set_attribute(vcsno, attr);
            break;
        }
        if ((40 <= arg1) && (arg1 <= 47)) {
            /* set background color */
            attr &= 0x0f;
            attr |= ecma48_to_vga_color[arg1 - 40];
            break;
        }
        logmsgf("%s: unknown graphic mode \\x%x\n", funcname, arg1);
        break;
      default:
        logmsgf("%s: unknown sequence CSI \\x%x\n", funcname, (uint)c);
    }
}

static int dev_tty_write(
    device  *dev, const char *buf, size_t buflen, size_t *written, off_t pos
) {
    UNUSED(pos);
    const char *funcname = __FUNCTION__;
    int arg1, arg2;
    bool hasarg1, hasarg2;

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
                vcsa_move_cursor_back(ttyvcs);
                vcsa_set_char(ttyvcs, ' ');
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
                  case 'H': /* HTS */
                    logmsgef("%s: TODO: set tabstop at current column", funcname);
                    break;
                  case 'M': /* RI -- reverse linefeed */
                    vcsa_move_cursor_by(ttyvcs, 0, -1);
                    break;
                  case '[': /* CSI -- Control Sequence Introducer */
                    ++s;
                    /* read params if present */
                    arg1 = 1; arg2 = 1;
                    hasarg1 = false; hasarg2 = false;
                    if (isdigit(*s)) {
                        hasarg1 = true;
                        do {
                            arg1 *= 10;
                            arg1 += (*s - '0');
                            ++s;
                        } while (isdigit(*s));
                    }
                    if (*s == ';') {
                        hasarg2 = true;
                        do {
                            arg2 *= 10;
                            arg2 += (*s - '0');
                            ++s;
                        } while (isdigit(*s));
                    }

                    ecma48_csi(*s, ttyvcs,
                               (hasarg1 ? &arg1 : NULL),
                               (hasarg2 ? &arg2 : NULL));
                    break;
                  default:
                    logmsgf("%s: unknown sequence ESC \\x%x\n", funcname, (uint)*s);
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



static int dev_tty_read(
        device *dev, char *buf, size_t buflen, size_t *written, off_t pos)
{
    UNUSED(pos);
    const char *funcname = __FUNCTION__;

    struct tty_device *tty = dev->dev_data;
    return_err_if(!tty, EKERN, "%s: no dev->dev_data\n", funcname);

    struct termios *tios = &tty->tty_conf;

    if (tios->c_iflag & ICANON) {
        /* canonical mode: serve a line */
        int eol_at;

        while (true) {
            eol_at = tty_inpq_strchr(&tty->tty_inpq, '\n');
            if (eol_at >= 0)
                break;

            /* tty_inpq may change here */
            cpu_halt();
        }

        //logmsgdf(".");
        size_t to_pop = (size_t)eol_at + 1;

        if (to_pop > buflen)
            to_pop = buflen;

        size_t nread = tty_inpq_pop((tty_inpqueue *)&tty->tty_inpq, buf, to_pop);

        if (written) *written = nread;
        return 0;
    }

    /* noncanonical mode */
    logmsgef("%s: ETODO: noncanonical mode", funcname);

    return ETODO;
}

int tty_read(mindev_t ttyno, char *buf, size_t buflen, size_t *written) {
    const char *funcname = __FUNCTION__;

    device *dev = get_tty_device(ttyno);
    return_dbg_if(!dev, ENOENT,
            "%s: ttyno=%d\n", funcname, ttyno);

    return dev_tty_read(dev, buf, buflen, written, 0);
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

#define MAX_CHARS_FROM_SCAN     4

static int ecma48_code_from_scancode(uint8_t sc, char buf[]) {
    /* TODO: adapt qwerty_layout[] to multichar sequences */
    buf[0] = translate_from_scan(NULL, sc);
    return 1;
}

void tty_keyboard_handler(scancode_t sc) {
    const char *funcname = __FUNCTION__;
    int ret;
    tty_inpqueue *inpq;
    //logmsgdf("%s(scancode=%d)\n", funcname, (int)sc);

    tty_device *tty = theTTYlist[ theActiveTTY ];
    returnv_err_if(!tty, "%s: no active tty", funcname);

    if ((0x3b <= sc) && (sc <= 0x42)) {
        vcsa_switch(sc - 0x3b);
        return;
    }

    char buf[MAX_CHARS_FROM_SCAN];

    switch (tty->tty_kbmode) {
      case TTYKBD_RAW:
        buf[0] = (char)sc;

        inpq = (tty_inpqueue *)&tty->tty_inpq;
        tty_inpq_push(inpq, buf, 1);
        logmsgdf("%s: RAW mode, tty_inpq_push(0x%x)\n", funcname, (int)buf[0]);

        break;
      case TTYKBD_ANSI:
        ret = ecma48_code_from_scancode(sc, buf);
        if (ret == 0) return;
        if (!buf[0]) return;

        if (tty->tty_conf.c_iflag & ICANON) {
            /* limited editing capabilities */
            switch (buf[0]) {
              case '\b':
                if (tty_inpq_unpush((tty_inpqueue *)&tty->tty_inpq, 1))
                    tty_write(theActiveTTY, "\b", 1);
                return;
              case '\n': vcsa_newline(tty->tty_vcs); break;
              default: vcsa_cprint(tty->tty_vcs, buf[0]);
            }
        }

        inpq = (tty_inpqueue *)&tty->tty_inpq;
        tty_inpq_push(inpq, buf, ret);
        //logmsgdf("%s: ANSI mode, tty_inpq_push(0x%x)\n", funcname, (int)buf[0]);
        break;
      default:
        logmsgef("%s: unknown keyboard mode %d\n", funcname, tty->tty_kbmode);
        return;
    }
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
        dev->dev_data = (void *)tty;
        dev->dev_ops  = &tty_ops;

        tty->tty_conf = stty_sane;
        tty->tty_kbmode = TTYKBD_ANSI;
        tty->tty_size.wx = SCR_WIDTH;
        tty->tty_size.wy = SCR_HEIGHT;

        tty->tty_inpq.start = tty->tty_inpq.end = 0;

        theTTYlist[i] = tty;
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

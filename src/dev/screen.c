#include <dev/screen.h>
#include <fs/devices.h>

#include <arch/i386.h>

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/errno.h>

#include <cosec/log.h>

/***
  *     VGA 80x25 'driver'
 ***/

#define VIDEO_MEM           (KERN_OFF + 0x000B8000)
#define VIDEOBUF            ((uint8_t *)VIDEO_MEM)

/* global state */
int active_vcs = CONSOLE_VCSA;

struct {
    uint line, col;
    uint8_t attr;
} vcs_data[ N_VCSA_DEVICES ];

device vcs_devices[ N_VCSA_DEVICES ];
device vcsa_devices[ N_VCSA_DEVICES ];

char vcsa_buf[ 2 * SCR_WIDTH * SCR_HEIGHT * N_VCSA_DEVICES ];


#define GET_VCSA_BUF(n) (vcsa_buf + (2 * SCR_WIDTH * SCR_HEIGHT * n))

int vcs_current(void) {
    return active_vcs;
}

inline void move_hw_cursor(uint cursorX, uint cursorY) {
    uint temp = cursorY * SCR_WIDTH + cursorX;

    /* This sends a command to indicies 14 and 15 in the
    *  CRT Control Register of the VGA controller. These
    *  are the high and low bytes of the index that show
    *  where the hardware cursor is to be 'blinking'. */
    outb(0x3D4, 14);
    outb(0x3D5, temp >> 8);
    outb(0x3D4, 15);
    outb(0x3D5, temp);
}


inline void print_console_debug(const char *msg) {
    char *buf = (char *)VIDEOBUF + (SCR_HEIGHT - 1) * SCR_WIDTH * 2;
    const char *s = msg;
    while (true) {
        if ((s - msg) >= SCR_WIDTH) return;
        if (s[0] == 0) return;
        buf[0] = s[0];
        buf[1] = 0x40;
        buf += 2;
        ++s;
    }
}

#define HANG_IFNOT(cond, msg)       \
if (!(cond)) do {                   \
    print_console_debug(msg);       \
    cpu_hang();                     \
} while (0)

inline static void clear_vga_sym(char *buf, uint8_t attr) {
    buf[0] = ' '; buf[1] = attr;
}

inline void vcsa_set_attribute(int vcsno, uint8_t attr) {
    if (vcsno == VIDEOMEM_VCSA) vcsno = active_vcs;
    int vcsind = (vcsno < 0x80 ? vcsno : vcsno - 0x80);
    returnv_dbg_if(vcsind < 0 || N_VCSA_DEVICES <= vcsind, "");

    vcs_data[vcsno].attr = attr;
}

uint8_t vcsa_get_attribute(int vcsno) {
    if (vcsno == VIDEOMEM_VCSA) vcsno = active_vcs;

    int vcsind = (vcsno < 0x80 ? vcsno : vcsno - 0x80);
    if (!(0 <= vcsind && vcsind < N_VCSA_DEVICES))
        return VCSA_DEFAULT_ATTRIBUTE;

    return vcs_data[vcsind].attr;
}

void vcsa_set_cursor(int vcsno, int x, int y) {
    if (vcsno == VIDEOMEM_VCSA) vcsno = active_vcs;
    HANG_IFNOT((0 <= vcsno) && (vcsno < N_VCSA_DEVICES),
            "vcsa_set_cursor : vcsno");
    if (x >= SCR_WIDTH)  x = SCR_WIDTH - 1;
    if (y >= SCR_HEIGHT) y = SCR_HEIGHT - 1;

    if (x >= 0) vcs_data[vcsno].col = x;
    if (y >= 0) vcs_data[vcsno].line = y;

    if (vcsno == active_vcs) {
        move_hw_cursor(x, y);
    }
}

void vcsa_get_cursor(int vcsno, int *x, int *y) {
    if (vcsno == VIDEOMEM_VCSA) vcsno = active_vcs;
    HANG_IFNOT(0 <= vcsno && vcsno < N_VCSA_DEVICES, "vcsa_get_cursor");

    if (x) *x = vcs_data[vcsno].col;
    if (y) *y = vcs_data[vcsno].line;
}

inline void update_hw_cursor(void) {
    move_hw_cursor(vcs_data[active_vcs].col, vcs_data[active_vcs].line);
}

inline void vcsa_refresh(void) {
    memcpy(VIDEOBUF, GET_VCSA_BUF(active_vcs), SCR_HEIGHT * SCR_WIDTH * 2);
    update_hw_cursor();
}

void vcsa_erase_line(int vcsno, enum vcsa_eraser method) {
    int j;
    int x = vcs_data[vcsno].col;
    int y = vcs_data[vcsno].line;
    uint8_t attr = vcs_data[vcsno].attr;

    char *buf = GET_VCSA_BUF(vcsno) + 2 * SCR_WIDTH * y;

    for (j = 0; j < SCR_WIDTH; ++j) {
        if (j < x) {
            if (method != VCSA_ERASE_AFTER_CURSOR)
                clear_vga_sym(buf, attr);
        } else {
            if (method != VCSA_ERASE_BEFORE_CURSOR)
                clear_vga_sym(buf, attr);
        }

        buf += 2;
    }

    if (vcsno == active_vcs)
        vcsa_refresh();
}

void vcsa_erase_screen(int vcsno, enum vcsa_eraser method) {
    int i, j;
    int x = vcs_data[vcsno].col;
    int y = vcs_data[vcsno].line;
    uint8_t attr = vcs_data[vcsno].attr;

    char *buf = GET_VCSA_BUF(vcsno);

    for (i = 0; i < SCR_HEIGHT; ++i) {
        for (j = 0; j < SCR_WIDTH; ++j) {
            if (i < y) {
                if (method != VCSA_ERASE_AFTER_CURSOR)
                    clear_vga_sym(buf, attr);
            } else if (i > y) {
                if (method != VCSA_ERASE_BEFORE_CURSOR)
                    clear_vga_sym(buf, attr);
            } else {
                if (j < x) {
                    if (method != VCSA_ERASE_AFTER_CURSOR)
                        clear_vga_sym(buf, attr);
                } else {
                    if (method != VCSA_ERASE_BEFORE_CURSOR)
                        clear_vga_sym(buf, attr);
                }
            }
            buf += 2;
        }
    }

    if (vcsno == active_vcs)
        vcsa_refresh();
}

void vcsa_clear(int vcsno) {
    if (vcsno == VIDEOMEM_VCSA)
        vcsno = active_vcs;

    if (!((0 <= vcsno) && (vcsno < N_VCSA_DEVICES))) return;

    uint8_t attr = vcs_data[vcsno].attr;
    if (!attr)
        attr = vcs_data[vcsno].attr = VCSA_DEFAULT_ATTRIBUTE;

    vcsa_erase_screen(vcsno, VCSA_ERASE_WHOLE);
    vcsa_set_cursor(vcsno, 0, 0);

    if (vcsno == active_vcs)
        vcsa_refresh();
}

void vcsa_switch(int vcsno) {
    if (!(0 <= vcsno && vcsno < N_VCSA_DEVICES)) return;
    if (vcsno == active_vcs) return;

    active_vcs = vcsno;
    vcsa_refresh();
}


static void vcsa_scroll_lines(int vcsno, int lines) {
    if (vcsno == VIDEOMEM_VCSA) vcsno = active_vcs;

    int i, j;
    uint8_t *buf = (uint8_t *)(GET_VCSA_BUF(vcsno));
    uint8_t attr = vcs_data[vcsno].attr;

    for (i = 0; i < (SCR_HEIGHT - lines); ++i)
        for (j = 0; j < SCR_WIDTH; ++j) {
            buf[0] = buf[ 2 * lines * SCR_WIDTH ];
            buf[1] = buf[ 2 * lines * SCR_WIDTH + 1];
            buf += 2;
        }
    for (i = 0; i < lines; ++i) {
        for (j = 0; j < SCR_WIDTH; ++j) {
            clear_vga_sym((char *)buf, attr);
            buf += 2;
        }
    }
    if (vcsno == active_vcs) vcsa_refresh();
}

static void vcsa_scroll_line(int vcsno) {
    vcsa_scroll_lines(vcsno, 1);
}

void vcsa_move_cursor_by(int vcsno, int dx, int dy) {
    if (!(0 <= vcsno && vcsno < N_VCSA_DEVICES)) return;

    int ln = (int)vcs_data[vcsno].line;
    int col = (int)vcs_data[vcsno].col;

    int scr_ln = 0;
    if ((ln + dy) >= SCR_HEIGHT) {
        scr_ln = (ln + dy) - (SCR_HEIGHT - 1);
        ln = SCR_HEIGHT - 1;
    } else if ((ln + dy) < 0) {
        ln = 0;  /* TODO : scroll back? */
    } else {
        ln += dy;
    }

    if ((col + dx) >= SCR_WIDTH) {
        col = SCR_WIDTH - 1;
    } else if ((col + dx) < 0) {
        col = 0;
    } else {
        col += dx;
    }

    vcs_data[vcsno].line = ln;
    vcs_data[vcsno].col = col;

    if (scr_ln)
        vcsa_scroll_lines(vcsno, scr_ln);
}

inline void vcsa_newline(int vcsno) {
    uint ln = vcs_data[vcsno].line;

    /* wrap line */
    vcs_data[vcsno].col = 0;
    if ((ln + 1) < SCR_HEIGHT)
        vcs_data[vcsno].line = ln + 1; /* go down one line */
    else
        vcsa_scroll_line(vcsno); /* scroll the buffer */

    if (vcsno == active_vcs)
        update_hw_cursor();
}

inline void vcsa_move_cursor_back(int vcsno) {
    int x, y;
    vcsa_get_cursor(vcsno, &x, &y);
    if ((y == 0) && (x == 0)) return;
    if (x == 0)
        vcsa_set_cursor(vcsno, SCR_WIDTH - 1, y - 1);
    else
        vcsa_set_cursor(vcsno, x - 1, y);
}

void vcsa_move_cursor_tabstop(int vcsno) {
    int x, y;
    vcsa_get_cursor(vcsno, &x, &y);
    x = TAB_INDENT * (1 + x/TAB_INDENT);
    if (x >= SCR_WIDTH) x = SCR_WIDTH - 1;
    vcsa_set_cursor(vcsno, x, y);
}

inline void vcsa_move_cursor_next(int vcsno) {
    uint col = vcs_data[vcsno].col;
    uint ln = vcs_data[vcsno].line;

    ++col;
    if (col < SCR_WIDTH)
        /* move to the next position in the same line */
        vcs_data[vcsno].col = col;
    else
        vcsa_newline(vcsno);

    if (vcsno == active_vcs) update_hw_cursor();
}

void vcsa_set_char(int vcsno, char c) {
    if (vcsno == VIDEOMEM_VCSA)
        vcsno = active_vcs;

    HANG_IFNOT((0 <= vcsno) && (vcsno < N_VCSA_DEVICES), "vcsa_cprint failed()");

    uint8_t attr = vcsa_get_attribute(vcsno);

    uint ln = vcs_data[vcsno].line;
    uint col = vcs_data[vcsno].col;

    char *buf = GET_VCSA_BUF(vcsno);
    buf += 2 * (SCR_WIDTH * ln + col);

    buf[0] = c;
    buf[1] = vcs_data[vcsno].attr;
    if (vcsno == active_vcs) {
        char *vmem = (char *)VIDEOBUF + 2 * (SCR_WIDTH * ln + col);
        vmem[0] = buf[0];
        vmem[1] = buf[1];
    }
}

void vcsa_cprint(int vcsno, char c) {
    if (vcsno == VIDEOMEM_VCSA)
        vcsno = active_vcs;

    HANG_IFNOT((0 <= vcsno) && (vcsno < N_VCSA_DEVICES), "vcsa_cprint failed()");

    vcsa_set_char(vcsno, c);
    vcsa_move_cursor_next(vcsno);
}

static const char *vcsa_get_roline(device *dev, off_t line);
static char *vcsa_get_line(device *dev, off_t line);
static int vcsa_refresh_line(device *dev, off_t line);
static int vcsa_read(device *dev, char *buf, size_t buflen, size_t *written, off_t pos);
static int vcsa_write(device *dev, const char *buf, size_t buflen, size_t *written, off_t pos);

static size_t vcsa_linesize() { return SCR_WIDTH * 2; }
static off_t vcsa_lines() { return SCR_HEIGHT; }


static const char *vcsa_get_roline(device *dev, off_t line) {
    return (const char *)vcsa_get_line(dev, line);
}

static char *vcsa_get_line(device *dev, off_t line) {
    const char *funcname = "vcsa_get_line";
    return_dbg_if((dev->dev_no < 0x80) || (dev->dev_no >= (0x80 + N_VCSA_DEVICES)), NULL,
            "%s(devno=%d)\n", funcname, dev->devno);
    return_dbg_if((line < 0) || (SCR_HEIGHT <= line), NULL,
            "%s(line=%d)\n", funcname, (int)line);

    if (dev->dev_no == 0x80)
        return (char *)VIDEO_MEM + 2 * SCR_WIDTH * line;

    return GET_VCSA_BUF(dev->dev_no - 0x80);
}

static int vcsa_refresh_line(device *dev, off_t line) {
    const char *funcname = "vcsa_refresh_line";
    return_dbg_if((dev->dev_no < 0x80) || (dev->dev_no >= (0x80 + N_VCSA_DEVICES)), ENODEV,
            "%s(devno=%d)\n", funcname, dev->devno);
    return_dbg_if((line < 0) || (SCR_HEIGHT <= line), ENXIO,
            "%s(line=%d)\n", funcname, (int)line);

    if (dev->dev_no - 0x80 == (mindev_t)active_vcs) {
        size_t bufoffset = 2 * SCR_WIDTH * line;
        char *vmem = (char *)VIDEO_MEM + bufoffset;
        char *bmem = GET_VCSA_BUF(dev->dev_no - 0x80) + bufoffset;
        memcpy(vmem, bmem, 2 * SCR_WIDTH);
    }
    return 0;
}

static int vcsa_read(device *dev, char *buf, size_t buflen, size_t *written, off_t pos) {
    const char *funcname = "vcsa_refresh_line";
    int vcs_index = (dev->dev_no >= 0x80 ? dev->dev_no - 0x80 : dev->dev_no);
    return_dbg_if((vcs_index < 0) || (vcs_index >= N_VCSA_DEVICES), ENODEV,
            "%s(devno=%d)\n", funcname, dev->dev_no);

    size_t scrsize = SCR_WIDTH * SCR_HEIGHT;
    size_t vsize = 2 * scrsize;

    bool vcsa_mode = (dev->dev_no < 0x80 ? false : true);
    size_t vpos = (vcsa_mode ? pos : 2 * pos);
    if (vpos > vsize) {
        if (written) *written = 0;
        return ENXIO;
    }

    char *vbuf = (vcs_index > 0 ? (char *)VIDEO_MEM : GET_VCSA_BUF(vcs_index)) + vpos;

    size_t bytes_to_read = buflen;
    if (vcsa_mode) {
        if (vpos + buflen > vsize)
            bytes_to_read = vsize - vpos;
    } else {
        if (pos + buflen > scrsize)
            bytes_to_read = scrsize - pos;
    }

    size_t bytes_done = 0;
    while (bytes_done < bytes_to_read) {
        *buf = *vbuf;
         ++buf; ++vbuf;
         if (!vcsa_mode) ++vbuf; /* skip cell attrs */
         ++bytes_done;
    }

    if (written) *written = bytes_done;
    return 0;
}

static int vcsa_write(device *dev, const char *buf, size_t buflen, size_t *written, off_t pos) {
    const char *funcname = "vcsa_write";

    int vcs_index = (dev->dev_no >= 0x80 ? dev->dev_no - 0x80 : dev->dev_no);
    return_dbg_if((vcs_index < 0) || (vcs_index >= N_VCSA_DEVICES), ENODEV,
            "%s(devno=%d)\n", funcname, dev->dev_no);

    const size_t scrsize = SCR_WIDTH * SCR_HEIGHT;
    const size_t vsize = 2 * scrsize;

    const bool vcsa_mode = (dev->dev_no < 0x80 ? false : true);
    const size_t vpos = (vcsa_mode ? pos : 2 * pos);
    if (vpos > vsize) {
        if (written) *written = 0;
        return ENXIO;
    }

    char *vbuf = (vcs_index > 0 ? (char *)VIDEO_MEM : GET_VCSA_BUF(vcs_index)) + vpos;

    size_t bytes_to_write = buflen;
    if (vcsa_mode) {
        if (vsize < vpos + bytes_to_write)
            bytes_to_write = vsize - vpos;
    } else {
        if (scrsize < pos + bytes_to_write)
            bytes_to_write = scrsize - pos;
    }

    size_t bytes_done = 0;
    while (bytes_done < bytes_to_write) {
        *vbuf = *buf;
        ++vbuf; ++buf;
        if (!vcsa_mode) ++vbuf;
        ++bytes_done;
    }

    if (written) *written = bytes_done;
    return 0;
}


/*
 *   VCS devices family
 */

struct device_operations vcs_ops = {
    .dev_get_roblock = NULL,
    .dev_get_rwblock = NULL,
    .dev_forget_block = NULL,
    .dev_size_of_block = NULL,
    .dev_size_in_blocks = NULL,

    .dev_read_buf = vcsa_read,
    .dev_write_buf = vcsa_write,
    .dev_has_data = NULL, /* always available for reading */

    .dev_ioctlv = NULL,
};

struct device_operations vcsa_ops = {
    /* VCSA as a block device interpretation:
     *  A VCSA block corresponds to a line; */
    .dev_get_roblock = vcsa_get_roline,
    .dev_get_rwblock = vcsa_get_line,
    .dev_forget_block = vcsa_refresh_line,
    .dev_size_of_block = vcsa_linesize,
    .dev_size_in_blocks = vcsa_lines,

    .dev_read_buf = vcsa_read,
    .dev_write_buf = vcsa_write,
    .dev_has_data = NULL,  /* always available for reading */

    .dev_ioctlv = NULL,
};

static device * get_vcs_device(mindev_t dev_no) {
    if (dev_no < N_VCSA_DEVICES) {
        return vcs_devices + dev_no;
    }
    if ((0x80 <= dev_no) && (dev_no < 0x80 + N_VCSA_DEVICES)) {
        return vcsa_devices - 0x80 + dev_no;
    }
    return NULL;
}

void init_vcs_devices(void) {
    int i, j;
    active_vcs = CONSOLE_VCSA;

    for (i = 0; i < N_VCSA_DEVICES; ++i) {
        /* init /dev/vcsI, /dev/vcsaI */
        device *vcs_dev = vcs_devices + i;
        device *vcsa_dev = vcsa_devices + i;

        vcs_dev->dev_type = vcsa_dev->dev_type = DEV_CHR;
        vcs_dev->dev_clss = vcsa_dev->dev_clss = CHR_VCS;

        vcs_dev->dev_no = i;
        vcsa_dev->dev_no = i + 0x80;

        vcs_dev->dev_ops = &vcs_ops;
        vcsa_dev->dev_ops = &vcsa_ops;

        vcs_data[i].attr = VCSA_DEFAULT_ATTRIBUTE;

        if (i == active_vcs) {
            memcpy(GET_VCSA_BUF(i), VIDEOBUF, SCR_HEIGHT * SCR_WIDTH * 2);
            continue;
        }

        vcsa_clear(i);
    }
}

devclass vcs_device_class = {
    .dev_type       = DEV_CHR,
    .dev_maj        = CHR_VCS,
    .dev_class_name = "VGA text buffers",

    .get_device     = get_vcs_device,
    .init_devclass  = init_vcs_devices,
};

devclass * get_vcs_devclass(void) {
    return &vcs_device_class;
}


/*
 *  Console printing
 */

void cprint(char c) {
    int x, y;
    switch (c) {
        case '\n':
            vcsa_newline(CONSOLE_VCSA);
            break;
        case '\r':
            vcsa_set_cursor(CONSOLE_VCSA, 0, VCS_DO_NOT_MOVE);
            break;
        case '\t':
            vcsa_move_cursor_tabstop(CONSOLE_VCSA);
            break;
        case '\b':
            vcsa_move_cursor_back(CONSOLE_VCSA);
            break;
        default:
            vcsa_cprint(CONSOLE_VCSA, c);
    }

    if (CONSOLE_VCSA == active_vcs)
        update_hw_cursor();
}

#define KBUFLEN    SCR_WIDTH * SCR_HEIGHT

int k_vprintf(const char *fmt, va_list ap) {
    if (!fmt) return 0;

    char buf[KBUFLEN];

    vsnprintf(buf, KBUFLEN, fmt, ap);
    buf[KBUFLEN - 1] = '\0';

    char *s = buf;
    while (s[0]) {
        cprint(s[0]);
        ++s;
    }

    if (active_vcs == CONSOLE_VCSA)
        vcsa_refresh();

    return 0;
}

int k_printf(const char *fmt, ...) {
    int ret;
    va_list args;
    va_start(args, fmt);
    ret = k_vprintf(fmt, args);
    va_end(args);
    return ret;
}

void print_centered(const char *s) {
    int margin = (SCR_WIDTH - strlen(s))/2;
    k_printf("\r");
    int i;
    for (i = 0; i < margin; ++i) k_printf(" ");
    k_printf("%s\n", s);
}


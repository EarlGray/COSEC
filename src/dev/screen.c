#include <dev/screen.h>
#include <fs/devices.h>

#include <arch/i386.h>
#include <log.h>

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>


/***
  *     VGA 80x25 'driver'
 ***/

#define VIDEO_MEM	        0x000B8000
#define VIDEOMEM	        ((uint8_t *)VIDEO_MEM)

#define DEFAULT_CURSOR_ATTR 0x07

static uint16_t cursorX = 0;
static uint16_t cursorY = 0;
static uint8_t  cursorAttr = DEFAULT_CURSOR_ATTR;

inline uint16_t get_cursor_x(void) {    return cursorX;   }
inline uint16_t get_cursor_y(void) {    return cursorY;   }
inline uint8_t get_cursor_attr()   {    return cursorAttr; }

inline void set_cursor_x(uint16_t X) {  cursorX = X;  }
inline void set_cursor_y(uint16_t Y) {  cursorY = Y;  }
inline void set_cursor_attr(uint8_t attr) {  cursorAttr = attr;   }
inline void set_default_cursor_attr(void) {  cursorAttr = DEFAULT_CURSOR_ATTR; }

inline void move_cursor_next_line();
inline void move_cursor_to(uint16_t cx, uint16_t cy);
void scroll_up_line();

void update_hw_cursor(void)
{
    unsigned temp = cursorY * SCR_WIDTH + cursorX;

    /* This sends a command to indicies 14 and 15 in the
    *  CRT Control Register of the VGA controller. These
    *  are the high and low bytes of the index that show
    *  where the hardware cursor is to be 'blinking'. */
    outb(0x3D4, 14);
    outb(0x3D5, temp >> 8);
    outb(0x3D4, 15);
    outb(0x3D5, temp);
}


void clear_screen(void) {
    uint8_t *pos = VIDEOMEM;
    int i;
    for (i = 0; i < (SCR_WIDTH * SCR_HEIGHT); ++i)
    {
        pos[0] = 0x20;
        pos[1] = cursorAttr;
        pos += 2;
    }
    move_cursor_to(0, 0);
}


inline void move_cursor_next() {
    ++cursorX;
    if (cursorX >= SCR_WIDTH) {
        cursorX = 0;
        move_cursor_next_line();
    }
    update_hw_cursor();
}

inline void move_cursor_to(uint16_t cx, uint16_t cy) {
    set_cursor_x(cx);
    set_cursor_y(cy);
    update_hw_cursor();
}

inline void move_cursor_next_line() {
    if (cursorY + 1 >= SCR_HEIGHT)
        scroll_up_line();
    else ++cursorY;
}

void scroll_up_line() {
    uint8_t *s = (uint8_t *)VIDEO_MEM;
    int i, j;
    for (i = 0; i < SCR_HEIGHT-1; ++i)
        for (j = 0; j < SCR_WIDTH; ++j)
        {
            (*s) = *(s + SCR_WIDTH + SCR_WIDTH);
            ++s;
            (*s) = *(s + SCR_WIDTH + SCR_WIDTH);
            ++s;
        }
    for (i = 0; i < SCR_WIDTH; ++i) {
        *s = 0x20;
        ++s;
        *s = cursorAttr;
        ++s;
    }
}

inline void set_char_at(int x, int y, char c, char attr) {
    uint8_t* p = (uint8_t *)( VIDEO_MEM + ((x + y * SCR_WIDTH) << 1) );
    p[0] = c;
    p[1] = attr;
}

inline void cprint(char c) {
    switch (c) {
    case '\n':
        set_cursor_x(0);
        move_cursor_next_line();
        break;
    case '\r':
        set_cursor_x(0);
        break;
    case '\t':
        set_cursor_x(TAB_INDENT * (1 + get_cursor_x()/TAB_INDENT));
        break;
    case '\b':
        if ((get_cursor_x() == 0) && (get_cursor_y() != 0))
            move_cursor_to(SCR_WIDTH - 1, get_cursor_y()-1);
        else
            set_cursor_x(get_cursor_x() - 1);
        break;
    default:
        set_char_at(get_cursor_x(), get_cursor_y(), c, get_cursor_attr());
        move_cursor_next();
    };
}


/*
 *   k_printf
 */

inline char get_digit(uint8_t digit) {
    if (digit < 10) return('0' + digit);
    else return('A' + digit - 10);
}

void print_uint(uint x, uint8_t base);

void print_int(int x, uint8_t base) {
    if (x < 0) {
        cprint('-');
        x = -x;
    }
    print_uint(x, base);
}

void print_uint(uint x, uint8_t base) {
#define maxDigits 32    // 4 * 8 max for binary
    char a[maxDigits] = { 0 };
    uint8_t n = maxDigits;
    if (x == 0) {
        --n;
        a[n] = '0';
    } else
    while (x != 0) {
        --n;
        a[n] = get_digit(x % base);
        x /= base;
    }
    while (n < maxDigits) {
        cprint(a[n]);
        n++;
    }
}

#define CC_NUL 0x00
#define CC_BEL '\a' /* 0x07 */
#define CC_BS  '\b' /* 0x08 */
#define CC_HT  '\t' /* 0x09 */
#define CC_LF  '\n' /* 0x0a */
#define CC_VT  0x0b
#define CC_FF  0x0c
#define CC_CR  '\r' /* 0x0d */
#define CC_SO  0x0e
#define CC_SI  0x0f
#define CC_CAN 0x18
#define CC_SUB 0x1a
#define CC_ESC 0x1b /* ESC [ */
#define CC_DEL 0x7f
#define CC_CSI 0x9b

void ecma48_control_sequence(uint num) {
    if (num == 0) {
        /* reset attributes */
        set_cursor_attr(DEFAULT_CURSOR_ATTR);
    } else if ((0x30 <= num) && (num <= 0x3f)) {
        /* set foreground color */
        uint8_t attr = get_cursor_attr();
        set_cursor_attr((attr & 0xF0) | (num - 0x30));
    } else if ((0x40 <= num) && (num <= 0x4f)) {
        /* set background color */
        uint8_t attr = get_cursor_attr();
        set_cursor_attr((attr & 0x0F) | ((num - 0x40) << 4));
    }
}

const char * ecma48_console_codes(const char *s) {
    switch (*s) {
    case CC_LF: case CC_VT: case CC_FF:
        set_cursor_x(0);
        move_cursor_next_line();
        break;
    case CC_CR:
        set_cursor_x(0);
        break;
    case CC_HT: {
        uint pos = TAB_INDENT * (1 + get_cursor_x()/TAB_INDENT);
        if (pos >= SCR_WIDTH) pos = SCR_WIDTH - 1;
        set_cursor_x(pos);
        }
        break;
    case CC_BS:
        if ((get_cursor_x() == 0) && (get_cursor_y() != 0))
            move_cursor_to(SCR_WIDTH - 1, get_cursor_y()-1);
        else
            set_cursor_x(get_cursor_x() - 1);
        break;

    case CC_ESC:
        switch (*++s) {
        case '[': {
            uint code = 0;
            s = sscan_uint(++s, &code, 16);
            if (*s == 'm') {
                ecma48_control_sequence(code);
                break;
            }
            } /* fall through */
        default:
            cprint(*s);
        }
        break;

    default:
        return s;
    }
    return ++s;
}

void k_vprintf(const char *fmt, va_list ap) {
    if (!fmt) return;

    const char *s = fmt;
    while (*s) {
        const char *sn = ecma48_console_codes(s);
        if (sn != s) {
            s = sn;
            continue;
        }

        switch (*s) {
        case '%':
            ++s;
            if (*s == 'l') ++s;
            switch (*s) {
            case 'd':
                print_int(va_arg(ap, int), 10);
                break;
            case 'u':
                print_uint(va_arg(ap, uint), 10);
                break;
            case 'x':
                print_uint(va_arg(ap, uint), 16);
                break;
            case 'o':
                print_uint(va_arg(ap, uint), 8);
                break;
            case 'c':
                cprint(va_arg(ap, int));
                break;
            case 's': {
                ptr_t ptr = va_arg(ap, ptr_t);
                char *c = (char *)ptr;
                while (*c)
                    cprint(*(c++));
                } break;
            default:
                cprint(*s);
            }
            break;
        default:
            cprint(*s);
        }
        ++s;
    }
    update_hw_cursor();
}

void k_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    k_vprintf(fmt, args);
    va_end(args);
}

void print_centered(const char *s) {
    int margin = (SCR_WIDTH - strlen(s))/2;
    k_printf("\r");
    int i;
    for (i = 0; i < margin; ++i) k_printf(" ");
    k_printf("%s\n", s);
}


/*
 *   VCS devices family
 */

/* global state */
int active_vcs;

struct {
    uint line, col;
} vcs_data[ N_VCSA_DEVICES ];

device vcs_devices[ N_VCSA_DEVICES ];
device vcsa_devices[ N_VCSA_DEVICES ];

char vcsa_buf[ 2 * SCR_WIDTH * SCR_HEIGHT * (N_VCSA_DEVICES - 1) ];


#define GET_VCSA_BUF(n) (vcsa_buf + (2 * SCR_WIDTH * SCR_HEIGHT * (n - 1)))


void vcsa_set_attribute(int vcsno, uint8_t attr) {
    returnv_dbg_if(vcsno <= 0 || N_VCSA_DEVICES <= vcsno, "");

    char *buf = (vcsno ? GET_VCSA_BUF(vcsno) : (char *)VIDEO_MEM);
    buf[ 2 * (SCR_WIDTH * vcs_data[vcsno].line + vcs_data[vcsno].col) + 1 ] = attr;
}

void vcsa_get_attribute(int vcsno, uint8_t *attr) {
    if (!(0 <= vcsno && vcsno < N_VCSA_DEVICES)) return;

    char *buf = (vcsno ? GET_VCSA_BUF(vcsno) : (char *)VIDEO_MEM);
    if (attr)
        *attr = buf[ 2 * (SCR_WIDTH * vcs_data[vcsno].line + vcs_data[vcsno].col) + 1 ];
}

void vcsa_set_cursor(int vcsno, int x, int y) {
    if (!(0 <= vcsno && vcsno < N_VCSA_DEVICES)) return;

    vcs_data[vcsno].col = x;
    vcs_data[vcsno].line = y;

    if (vcsno == active_vcs)
        vcsa_set_cursor(0, x, y);
    if (vcsno == 0)
        move_cursor_to(x, y);
}

void vcsa_get_cursor(int vcsno, int *x, int *y) {
    if (!(0 <= vcsno && vcsno < N_VCSA_DEVICES)) return;

    if (x) *x = vcs_data[vcsno].col;
    if (y) *y = vcs_data[vcsno].line;
}

void vcsa_clear(int vcsno) {
    if (!(0 <= vcsno && vcsno < N_VCSA_DEVICES)) return;
    int i;

    char *buf = (vcsno ? GET_VCSA_BUF(vcsno) : (char *)VIDEO_MEM);
    uint8_t attr;

    vcsa_get_attribute(vcsno, &attr);

    for (i = 0; i < SCR_WIDTH * SCR_HEIGHT; ++i) {
        buf[ 2 * i ] = ' ';
        buf[ 2 * i + 1] = attr;
    }

    vcs_data[vcsno].col = vcs_data[vcsno].line = 0;

    if (vcsno == active_vcs)
        vcsa_clear(0);
}

void vcsa_switch(int vcsno) {
    if (!(0 < vcsno && vcsno < N_VCSA_DEVICES)) return;
    if (vcsno == active_vcs) return;

    memcpy(VIDEOMEM, GET_VCSA_BUF(vcsno), SCR_WIDTH * SCR_HEIGHT * 2);
    vcsa_set_cursor(0, vcs_data[vcsno].col, vcs_data[vcsno].line);

    active_vcs = vcsno;
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
    memset(vcsa_buf, 0, sizeof(vcsa_buf));

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

        if (i == 0) continue;

        char *buf = (i ? GET_VCSA_BUF(i) : (char *)VIDEO_MEM);
        for (j = 0; j < SCR_HEIGHT * SCR_WIDTH; ++j) {
            buf[ 2 * j ] = ' ';
            buf[ 2 * j + 1] = cursorAttr;
        }
    }

    active_vcs = 1;
}

devclass vcs_device_class = {
    .dev_type       = DEV_CHR,
    .dev_maj        = CHR_VCS,
    .dev_class_name = "VGA text buffers",

    .get_device     = get_vcs_device,
    .init_devclass  = init_vcs_devices,
};

devclass * get_vcs_family(void) {
    return &vcs_device_class;
}

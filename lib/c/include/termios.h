#ifndef __COSEC_TERMIOS_H__
#define __COSEC_TERMIOS_H__

#include <stdint.h>

/*
 *  Descriptions taken from BSD's termios(4)
 */

#define MAX_TTY     64
#define MAX_TTYS    4
#define MAX_INPUT   512

/* c_iflag */
#define IGNBRK          0x00000001      /* ignore BREAK condition */
#define BRKINT          0x00000002      /* map BREAK to SIGINTR */
#define IGNPAR          0x00000004      /* ignore (discard) parity errors */
#define PARMRK          0x00000008      /* mark parity and framing errors */
#define INPCK           0x00000010      /* enable checking of parity errors */
#define ISTRIP          0x00000020      /* strip 8th bit off chars */
#define INLCR           0x00000040      /* map NL into CR */
#define IGNCR           0x00000080      /* ignore CR */
#define ICRNL           0x00000100      /* map CR to NL (ala CRMOD) */
#define IXON            0x00000200      /* enable output flow control */
#define IXOFF           0x00000400      /* enable input flow control */
#define IXANY           0x00000800      /* any char will restart after stop */
#define IMAXBEL     0x00002000  /* ring bell on input queue full */
#define IUTF8       0x00004000  /* maintain state for UTF-8 VERASE */

/* c_oflag */
#define OPOST           0x00000001      /* enable following output processing */
#define ONLCR           0x00000002      /* map NL to CR-NL (ala CRMOD) */
#define OXTABS          0x00000004      /* expand tabs to spaces */
#define ONOEOT          0x00000008      /* discard EOT's (^D) on output) */

/* c_cflag */
#define SIZE       0x00000300  /* character size mask */
#define     CS5         0x00000000      /* 5 bits (pseudo) */
#define     CS6         0x00000100      /* 6 bits */
#define     CS7         0x00000200      /* 7 bits */
#define     CS8         0x00000300      /* 8 bits */
#define CSTOPB      0x00000400  /* send 2 stop bits */
#define CREAD       0x00000800  /* enable receiver */
#define PARENB      0x00001000  /* parity enable */
#define PARODD      0x00002000  /* odd parity, else even */
#define HUPCL       0x00004000  /* hang up on last close */
#define CLOCAL      0x00008000  /* ignore modem status lines */

/* c_lflag */
#define ECHOKE      0x00000001  /* visual erase for line kill */
#define ECHOE       0x00000002  /* visually erase chars */
#define ECHOK       0x00000004  /* echo NL after line kill */
#define ECHO        0x00000008  /* enable echoing */
#define ECHONL      0x00000010  /* echo NL even if ECHO is off */
#define ECHOPRT     0x00000020  /* visual erase mode for hardcopy */
#define ECHOCTL     0x00000040  /* echo control chars as ^(Char) */
#define ISIG        0x00000080  /* enable signals INTR, QUIT, [D]SUSP */
#define ICANON      0x00000100  /* canonicalize input lines */
#define ALTWERASE   0x00000200  /* use alternate WERASE algorithm */
#define IEXTEN      0x00000400  /* enable DISCARD and LNEXT */
#define EXTPROC     0x00000800  /* external processing */
#define TOSTOP      0x00400000  /* stop background jobs from output */
#define FLUSHO      0x00800000  /* output being flushed (state) */
#define NOKERNINFO  0x02000000  /* no kernel output from VSTATUS */
#define PENDIN      0x20000000  /* XXX retype pending input (state) */
#define NOFLSH      0x80000000  /* don't flush after interrupt */

/* c_cc indexes */
#define VEOF        0
#define VEOL        1
#define VEOL2       2
#define VERASE      3
#define VKILL       5
#define VREPREINT   6
#define VINTR       8
#define VQUIT       9
#define VSUSP       10
#define VSTART      12
#define VSTOP       13
#define VMIN        16
#define VTIME       17

#define NCCS    20

/* c_?speed */
#define B0          0
/* unsupported: B50 - B4800 */
#define B9600       9600
#define B19200      19200
#define B38400      38400


typedef unsigned long   tcflag_t;
typedef unsigned char   cc_t;
typedef unsigned long   speed_t;


struct termios {
    tcflag_t    c_iflag;        /* input flags */
    tcflag_t    c_oflag;        /* output flags */
    tcflag_t    c_cflag;        /* control flags */
    tcflag_t    c_lflag;        /* local flags */
    cc_t        c_cc[NCCS];     /* control chars */
    speed_t     c_ispeed;       /* input speed */
    speed_t     c_ospeed;       /* output speed */
};

struct winsize {
    uint16_t    wx;
    uint16_t    wy;
};

enum tty_kbdmode {
    TTYKBD_RAW = 0,
    TTYKBD_ANSI = 1,

    TTYKBD_LAST,
};

enum tty_ioctl {
    KDGKBMODE,
    KDSKBMODE,
};

#endif //__COSEC_TERMIOS_H__

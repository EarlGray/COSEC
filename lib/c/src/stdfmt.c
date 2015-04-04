#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/errno.h>

typedef bool (*putc_f)(int, void *);

int vgprintf(putc_f gputc, void *putc_arg, const char *format, va_list ap);

int gprintf(putc_f gputc, void *putc_arg, const char *fmt, ...) {
    int ret = -EAGAIN;
    va_list ap;
    va_start(ap, fmt);
    ret = vgprintf(gputc, putc_arg, fmt, ap);
    va_end(ap);
    return ret;
}

#define PUTC(chr)   do {                    \
    if (!gputc(chr, putc_arg)) return ret;  \
    ++ret;                                  \
} while (0)

enum format_flags {
    FMTFLAG_ALTERNATIVE_FORM = 0x0001, /* '#' */
    FMTFLAG_ZERO_PADDED      = 0x0002, /* '0' */
    FMTFLAG_BLANK_SIGN       = 0x0004, /* ' ' */
    FMTFLAG_PLUS_SIGN        = 0x0008, /* '+' */
    FMTFLAG_RIGHT_ALIGN      = 0x0010, /* '-' */
    FMTFLAG_THOUSAND_GROUPS  = 0x0020, /* \'  */
    FMTFLAG_DONE             = 0x8000, /* when all flags are read */
};

enum format_length_modifier {
    FMTLENMOD_SHORTSHORT  = 0x01,  /* hh */
    FMTLENMOD_SHORT       = 0x02,  /* h  */
    FMTLENMOD_LONG        = 0x03,  /* l  */
    FMTLENMOD_LONGLONG    = 0x04,  /* ll */
    FMTLENMOD_LONGDOUBLE  = 0x05,  /* L  */
    FMTLENMOD_SIZET       = 0x06,  /* z  */
    FMTLENMOD_PTRDIFFT    = 0x07,  /* t  */
};

struct fmtformat {
    long field_width;
    long precision;
    enum format_length_modifier typemod;
    unsigned flags;
};

int vgprintf(putc_f gputc, void *putc_arg, const char *format, va_list ap) {
    if (!format) return 0;
    if (!gputc) return -EINVAL;

    int ret = 0;
    char *tmp;
    const char *fmtstart;
    
    long argpos = 0;
    struct fmtformat fmtprm = {
        .field_width = 0,
        .precision = 0,
        .typemod = 0,
        .flags = 0,
    };

    const char *fmt = format;
    while (*fmt) {
        switch (*fmt) {
            /* formatters */
            case '%':
                fmtstart = fmt;

                /* possible arg position specifier */
                tmp = NULL;
                argpos = strtol(fmt + 1, &tmp, 10);
                if ((tmp != fmt) && (tmp[0] == '$')) {
                    fmt = tmp; ++fmt; // number & dollar
                } else {
                    argpos = 0; // some number, but not a position specifier
                }

                /* read format flags */
                fmtprm.flags = 0;
                do {
                    switch (*++fmt) {
                      case '#': fmtprm.flags |= FMTFLAG_ALTERNATIVE_FORM; break;
                      case '0': fmtprm.flags |= FMTFLAG_ZERO_PADDED; break;
                      case ' ': fmtprm.flags |= FMTFLAG_BLANK_SIGN; break;
                      case '+': fmtprm.flags |= FMTFLAG_PLUS_SIGN; break;
                      case '-': fmtprm.flags |= FMTFLAG_RIGHT_ALIGN; break;
                      case '\'': fmtprm.flags |= FMTFLAG_THOUSAND_GROUPS; break;
                      default:  fmtprm.flags |= FMTFLAG_DONE;
                    }
                } while (!(fmtprm.flags & FMTFLAG_DONE));

                /* read min. field width if present */
                fmtprm.field_width = strtol(fmt, &tmp, 10);
                if (fmt != tmp) {
                    fmt = tmp;
                }

                /* possibly read precision */
                fmtprm.precision = 0;
                if (*fmt == '.') {
                    ++fmt;
                    fmtprm.precision = strtol(fmt, &tmp, 10);
                    if (fmt != tmp) {
                        fmt = tmp;
                    }
                }

                /* read type modifier */
                fmtprm.typemod = 0;
                switch (*fmt) {
                    case 'h':
                        if (fmt[1] == 'h') {
                            fmtprm.typemod = FMTLENMOD_SHORTSHORT; fmt += 2;
                        } else {
                            fmtprm.typemod = FMTLENMOD_SHORT; ++fmt;
                        }
                        break;
                    case 'l':
                        if (fmt[1] == 'l') {
                            fmtprm.typemod = FMTLENMOD_LONGLONG; fmt += 2;
                        } else {
                            fmtprm.typemod = FMTLENMOD_LONG; ++fmt;
                        }
                        break;
                    case 'L': fmtprm.typemod = FMTLENMOD_LONGDOUBLE; ++fmt; break;
                    case 'z': fmtprm.typemod = FMTLENMOD_SIZET;      ++fmt; break;
                    case 't': fmtprm.typemod = FMTLENMOD_PTRDIFFT;   ++fmt; break;
                }

                /* read the format specifier */
                switch (*fmt) {
                    case '%': PUTC('%'); break;
                    case 'd': case 'i': /* TODO */ break;
                    case 'u': /* TODO */  break;
                    case 'o': /* TODO */ break;
                    case 'x': case 'X': /* TODO */ break;
                    case 'e': case 'E': /* TODO */ break;
                    case 'f': case 'F': /* TODO */ break;
                    case 'g': case 'G': /* TODO */ break;
                    case 'a': case 'A': /* TODO */ break;
                    case 'c': /* TODO */ break;
                    case 's': /* TODO */ break;
                    case 'p': /* TODO */ break;
                    case 'n': /* TODO */ break;
                    default:
                       /* "formatting" does not apply, print as text */
                       while (fmtstart != fmt) {
                           PUTC(*fmtstart);
                           ++fmtstart;
                       }
                       PUTC(*fmt);
                }
                ++fmt;
                break;
            default:
                // just put a character
                PUTC(*fmt);
        }
        ++fmt;
    }
    return ret;
}
#undef PUTC

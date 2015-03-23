#include <stdio.h>

#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>

#include <dev/screen.h>
#include <dev/kbd.h>
#include <dev/tty.h>

#include <arch/i386.h>

#include <log.h>

#warning "TODO: fopen, freopen, fscanf, ..."
#pragma GCC diagnostic ignored "-Wunused-parameter"

extern int theErrNo;

struct FILE_struct { };

FILE f_stdin =  { };
FILE f_stdout = { };
FILE f_stderr = { };

FILE *stdin = &f_stdin;
FILE *stdout = &f_stdout;
FILE *stderr = &f_stderr;

/***
  *     I/O
 ***/

#define ZERO_PADDED     0x0001
#define PRINT_PLUS      0x0002
#define LEFT_PADDED     0x0004
#define SPACE_PLUS      0x0008

#define get_digit(digit) \
    ( ((digit) < 10) ? ('0' + (digit)) : ('A' + (digit) - 10))


char * snprint_uint(char *str, char *const end, uint x, uint8_t base, uint flags, int precision) {
#define maxDigits 32	// 4 * 8 max for binary
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

    if (~(flags & LEFT_PADDED)) {
        int indent = maxDigits - n;
        while (precision > indent) {
            --n;
            a[n] = (flags & ZERO_PADDED ? '0' : ' ');
            --precision;
        }
    }

    while (n < maxDigits) {
        if (end && (end <= str)) return end;
        *(str++) = a[n++];
    }

    if (flags & LEFT_PADDED) {
        while (precision > (maxDigits - n)) {
            *(str++) = ' ';
            --precision;
        }
    }

    return str;
}

char * snprint_int(char *str, char *const end, int x, uint8_t base, uint flags, int precision) {
    if (end && (end <= str)) return end;

   	if (x < 0) {
        *(str++) = '-';
        x = -x;
    } else if (flags & PRINT_PLUS)
        *(str++) = '+';
    else if (flags & SPACE_PLUS)
        *(str++) = ' ';

    return snprint_uint(str, end, x, base, flags, precision);
}

const char * sscan_uint(const char *str, uint *res, const uint8_t base) {
    *res = 0;

    do {
        char c = toupper(*str);
        if (('0' <= c) && (c <= '9')) {
            *res *= base;
            *res += (c - '0');
        } else
        if (('A' <= c) && (c <= ('A' + base - 10))) {
            *res *= base;
            *res += (c - 'A' + 10);
        } else
            return str;
        ++str;
    } while (1);
}

const char * sscan_int(const char *str, int *res, const uint8_t base) {
    char sign = 1;

    switch (*str) {
    case '-': sign = -1;
    case '+': ++str;
    }

    str = sscan_uint(str, (uint *)res, base);

    if (sign == -1)
        *res = - *res;
    return str;
}

int printf(const char *format, ...) {
    int ret;
    va_list ap;
    va_start(ap, format);
    ret = vfprintf(stdout, format, ap);
    va_end(ap);
    return ret;
}

int fprintf(FILE *stream, const char *format, ...) {
    int ret;
    va_list ap;
    va_start(ap, format);
    ret = vfprintf(stream, format, ap);
    va_end(ap);
    return ret;
}

int vfprintf(FILE *stream, const char *format, va_list ap) {
    int ret = 0;
    if (stream == stdout) {
        int bufsize = 64;
        char *buf = malloc(bufsize);
        while (1) {
            ret = vsnprintf(buf, bufsize, format, ap);
            if (ret + 1 < bufsize)
                break;

            bufsize *= 2;
            realloc(buf, bufsize);
        }

        k_printf("%s", buf);
        free(buf);
        return ret;
    }
    if (stream == stderr) {
        vcsa_set_attribute(CONSOLE_VCSA, 0x0C);
        ret = vfprintf(stdout, format, ap);
        vcsa_set_attribute(CONSOLE_VCSA, VCSA_DEFAULT_ATTRIBUTE);
        return ret;
    }

    logmsgef("TODO: vfprintf(*%x, '%s', ...)", (uint)stream, format);
    return ret;
}


int snprintf(char *str, size_t size, const char *format, ...) {
    int ret;
    va_list ap;
    va_start(ap, format);
    ret = vsnprintf(str, size, format, ap);
    va_end(ap);
    return ret;
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap) {
    const char *fmt_c = format;
    char *out_c = str;
    char *end = (size == 0 ? 0 : str + size - 1);

    while (*fmt_c) {
        if (end && (out_c >= end)) {
            *end = 0;
            return end - str;
        }

        switch (*fmt_c) {
        case '%': {
            int precision = 0;
            uint flags = 0;
            ++fmt_c;

            bool changed;
            do {
                changed = true;
                switch (*fmt_c) {
                case '0':   flags |= ZERO_PADDED; ++fmt_c; break;
                case '+':   flags |= PRINT_PLUS; ++fmt_c; break;
                case ' ':   flags |= SPACE_PLUS; ++fmt_c; break;
                case '.':
                    fmt_c = sscan_int(++fmt_c, &precision, 10);
                    break;
                default:
                    changed = false;
                }
            } while (changed);

            switch (*fmt_c) {
            case 'x': {
                int arg = va_arg(ap, int);
                out_c = snprint_uint(out_c, end, arg, 16, flags, precision);
                } break;
            case 'd': case 'i': {
                int arg = va_arg(ap, int);
                out_c = snprint_int(out_c, end, arg, 10, flags, precision);
                } break;
            case 'u': {
                uint arg = va_arg(ap, uint);
                out_c = snprint_uint(out_c, end, arg, 10, flags, precision);
                } break;
            case 's': {
                char *c = va_arg(ap, char *);
                while (*c) {
                    if (end && (out_c >= end)) break;
                    *(out_c++) = *(c++);
                }
                } break;
            case 'g': {
                double arg = va_arg(ap, double);
                uint intpart = (uint)arg;
                uint fracpart = (uint)(1000000.0 * (arg - intpart));
                out_c = snprint_uint(out_c, end, intpart, 10, flags, 0);
                if (fracpart > 0) {
                    *out_c++ = '.';
                    out_c = snprint_uint(out_c, end, fracpart, 10,
                                     ZERO_PADDED | LEFT_PADDED, 6);
                }
                } break;
            case 'p': {
                void *arg = va_arg(ap, void *);
                *out_c++ = '0'; *out_c++ = 'x';
                out_c = snprint_uint(out_c, end, (uint)arg, 16,
                                     ZERO_PADDED | flags, 8);
                } break;
            default:
                logmsgef("vsnprintf: unknown format specifier %x", (uint)*fmt_c);
                *out_c = *fmt_c;
                ++out_c;
            }
            } break;
        default:
            *out_c = *fmt_c;
            ++out_c;
        }
        ++fmt_c;
    }
    *out_c = 0;
    return out_c - str;
}

int vsprintf(char *str, const char *format, va_list ap) {
    return vsnprintf(str, 0, format, ap);
}

int sprintf(char *str, const char *format, ...) {
    va_list vl;
    va_start(vl, format);
    int res = vsnprintf(str, 0, format, vl);
    va_end(vl);
    return res;
}

int fscanf(FILE *stream, const char *format, ...) {
    logmsge("TODO: fscanf");
    return 0;
}


FILE * fopen(const char *path, const char *mode) {
    logmsge("TODO: fopen");
    return NULL;
}

FILE *freopen(const char *path, const char *mode, FILE *stream) {
    /* TODO */
    logmsge("TODO: freopen");
    return stream;
}

char *tmpnam(char *s) {
    logmsge("TODO: tmpnam('%s')", s);
    return "";
}

size_t fread(void *ptr, size_t size, size_t nmmeb, FILE *stream) {
    logmsge("TODO: fread");
    return 0;
}

size_t fwrite(const void *ptr, size_t size, size_t nitems, FILE *stream) {
    char const *cptr = ptr;
    size_t i;
    if (stream == stdout) {
        logmsgf("fwrite(*%x, %x, %x, stdout)\n", (uint)ptr, size, nitems);
        /* for (i = 0; i < size * nitems; ++i) { cprint(cptr[i]); } */
        tty_write(CONSOLE_TTY, ptr, size * nitems);
        return nitems;
    }
    if (stream == stderr) {
        logmsgf("fwrite(*%x, %x, %x, stderr)\n", (uint)ptr, size, nitems);
        vcsa_set_attribute(CONSOLE_VCSA, 4); // RED
        for (i = 0; i < size * nitems; ++i) {
            cprint(cptr[i]);
        }
        vcsa_set_attribute(CONSOLE_VCSA, VCSA_DEFAULT_ATTRIBUTE);
        return nitems;
    }
    logmsge("TODO: fwrite(*%x, %x, %x, stream=*%x)\n",
            (uint)ptr, size, nitems, (uint)stream);
    return 0;
}

FILE *tmpfile(void) {
    logmsge("TODO: tmpfile");
    return NULL;
}

int fgetc(FILE *f) {
    if (!f) return EOF;

    if (f == stdin)
        return kbd_getchar();

    return EOF;
}

int ungetc(int c, FILE *stream) {
    logmsge("TODO: ungetc");
    return EOF;
}

char *fgets(char *s, int size, FILE *stream) {
    logmsge("TODO: fgets");
    return NULL;
}

long ftell(FILE *stream) {
    logmsge("TODO: ftell");
    return -1;
}

int fseek(FILE *stream, long offset, int whence) {
    logmsge("TODO: fseek");
    return -1;
}

int fclose(FILE *fp) {
    theErrNo = ETODO;
    return EOF;
}

int fflush(__unused FILE *stream) {
    if (stream == stdout)
        return 0;
    if (stream == stderr)
        return 0;
    theErrNo = EBADF;
    return 0;
}

int setvbuf(FILE *stream, char *buf, int type, size_t size) {
    logmsge("TODO: setvbuf");
    return EOF;
}

int feof(FILE *stream) {
    if (stream == stdin)
        return 0;
    return 1;
}

int rename(const char *old, const char *new) {
    logmsge("TODO: rename");
    return -1;
}

int remove(const char *path) {
    theErrNo = ETODO;
    logmsge("TODO: remove");
    return -1;
}

void clearerr(__unused FILE *stream) {
    theErrNo = ETODO;
}

int ferror(__unused FILE *stream) {
    if (stream == stdin)  return 0;
    if (stream == stdout) return 0;
    if (stream == stderr) return 0;
    return 1;
}


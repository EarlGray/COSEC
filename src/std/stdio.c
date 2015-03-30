#include <stdio.h>

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>

#include <dev/screen.h>
#include <dev/kbd.h>
#include <dev/tty.h>
#include <fs/fs_sys.h>

#include <arch/i386.h>

#include <log.h>

extern int theErrNo;

struct FILE_struct {
    int file_fd;

    enum buffering_mode_t file_bufmode;

    size_t file_bufpos;
    size_t file_bufend;
    size_t file_bufsz;
    char *file_buf;
};

char stdinbuf[PAGE_SIZE];
char stdoutbuf[PAGE_SIZE];

FILE f_stdin =  {
    .file_fd = STDIN_FILENO,
    .file_bufmode = _IOFBF,
    .file_bufpos = 0,
    .file_bufend = 0,
    .file_bufsz = sizeof(stdinbuf),
    .file_buf = stdinbuf,
};
FILE f_stdout = {
    .file_fd = STDOUT_FILENO,
    .file_bufmode = _IOLBF,
    .file_bufpos = 0,
    .file_bufend = 0,
    .file_bufsz = sizeof(stdoutbuf),
    .file_buf = stdoutbuf,
};
FILE f_stderr = {
    .file_fd = STDERR_FILENO,
    .file_bufmode = _IONBF,
};

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
    logmsgdf("vsprintf(fd=%d, '%s', ...)\n", stream->file_fd, format);
    int ret = 0;

    int bufsize = 64;
    char *buf = malloc(bufsize);
    while (1) {
        ret = vsnprintf(buf, bufsize, format, ap);
        if (ret + 1 < bufsize)
            break;

        bufsize *= 2;
        buf = realloc(buf, bufsize);
    }

    if (stream == stdout)
        k_printf(buf);
    else if (stream == stderr) {
        vcsa_set_attribute(CONSOLE_TTY, VCSA_ATTR_RED);
        k_printf(buf);
        vcsa_set_attribute(CONSOLE_TTY, VCSA_DEFAULT_ATTRIBUTE);
    } else
        fwrite(buf, ret, 1, stream);
    free(buf);
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

FILE *tmpfile(void) {
    logmsge("TODO: tmpfile");
    return NULL;
}

FILE * fopen(const char *path, const char *mode) {
    theErrNo = 0;
    int flags = 0;

    if (strchr(mode, '+')) {
        flags |= O_RDWR;
        if (strchr(mode, 'w')) flags |= (O_CREAT|O_TRUNC);
        else if (strchr(mode, 'a')) flags |= (O_CREAT|O_APPEND);
    } else {
        if (strchr(mode, 'r')) flags |= O_RDONLY;
        else if (strchr(mode, 'w')) flags |= (O_WRONLY|O_CREAT|O_TRUNC);
        else if (strchr(mode, 'a')) flags |= (O_WRONLY|O_CREAT|O_APPEND);
    }

    int fd = sys_open(path, flags);
    if (fd < 0) {
        theErrNo = -fd;
        return NULL;
    }

    FILE *f = malloc(sizeof(FILE));
    if (!f) return NULL;

    /* init buffering */
    f->file_buf = kmalloc(PAGE_SIZE);
    f->file_bufmode = f->file_buf ? _IOFBF : _IONBF;
    f->file_bufsz = f->file_buf ? PAGE_SIZE : 0;
    f->file_bufpos = 0;
    f->file_bufend = 0;

    f->file_fd = fd;

    logmsgdf("fopen(%s): fd=%d\n", path, fd);
    return f;
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


static size_t fread_nobuf(char *ptr, size_t nbytes, FILE *f) {
    int ret = sys_read(f->file_fd, ptr, nbytes);
    if (ret < 0)
        return (size_t)ret;

    theErrNo = -ret;
    return 0;
}

static size_t fread_upto(char *ptr, size_t nbytes, FILE *f, char *upto) {
    const char *funcname = __FUNCTION__;
    char *buf = f->file_buf + f->file_bufpos;
    char *stop = NULL;
    size_t to_read;
    size_t nread = 0;

    to_read = f->file_bufend - f->file_bufpos;
    if (upto) {
        stop = strnchr(buf, to_read, upto[0]);
        if (stop) to_read = (stop - buf) + 1;
    }
    if (to_read > nbytes)
        to_read = nbytes;

    memcpy(ptr, buf, to_read);
    logmsgdf("%s: memcpy(,, %d)\n", funcname, to_read);
    f->file_bufpos += to_read;
    nread = to_read;

    if (stop) return nread;

    while (nread < nbytes) {
        buf = f->file_buf;
        int sysread = sys_read(f->file_fd, buf, f->file_bufsz);
        logmsgdf("%s: sys_read(%d, *%x, %d) -> %d\n",
                funcname, f->file_fd, (uint)buf, f->file_bufsz, sysread);
        if (sysread <= 0) {
            theErrNo = -sysread;
            return nread;
        }
        f->file_bufpos = 0;
        f->file_bufend = (size_t)sysread;

        if (upto)
            stop = strnchr(buf, f->file_bufend, upto[0]);

        to_read = (size_t)sysread;
        if ((nread + to_read) > nbytes)
            to_read = nbytes - nread;
        if (upto && stop)
            to_read = (stop - f->file_buf) + 1;

        logmsgdf("%s: memcpy(..., %d)\n", funcname, to_read);
        memcpy(ptr + nread, buf, to_read);
        nread += to_read;
        f->file_bufpos += to_read;

        if (stop) return nread;
    }

    return nread;
}

static size_t fread_fbf(char *ptr, size_t nbytes, FILE *f) {
    return fread_upto(ptr, nbytes, f, NULL);
}

static size_t fread_lbf(char *ptr, size_t nbytes, FILE *f) {
    char nl = '\n';
    return fread_upto(ptr, nbytes, f, &nl);
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    const char *funcname = __FUNCTION__;
    logmsgdf("fread(*%x, %d, fd=%d\n", (uint)ptr, size * nmemb, stream->file_fd);
    theErrNo = 0;

    switch (stream->file_bufmode) {
      case _IONBF: return fread_nobuf(ptr, size * nmemb, stream);
      case _IOFBF: return fread_fbf(ptr, size * nmemb, stream);
      case _IOLBF: return fread_lbf(ptr, size * nmemb, stream);
      default:
        logmsgef("%s: unknown file buffering mode %d\n", funcname, stream->file_bufmode);
    }
    return 0;
}


size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    logmsgdf("fwrite(*%x, %d, %d, fd=%d\n", (uint)ptr, size, nmemb, stream->file_fd);
    int ret = sys_write(stream->file_fd, ptr, size * nmemb);
    if (ret >= 0) return (size_t)ret;

    theErrNo = -ret;
    return 0;
}

int fgetc(FILE *f) {
    if (!f) return EOF;
    logmsgdf("fgetc\n");

    if (f == stdin)
        return kbd_getchar();

    return EOF;
}

int ungetc(int c, FILE *stream) {
    logmsge("TODO: ungetc");
    return EOF;
}

char *fgets(char *s, int size, FILE *stream) {
    theErrNo = 0;
    logmsgdf("fgets(*%x, %d, fd=%d)\n", (uint)s, size, stream->file_fd);
    char nl = '\n';
    size_t nread = fread_upto(s, size - 1, stream, &nl);
    if (nread < 1)
        return NULL;

    s[nread] = 0;
    logmsgdf("fgets: nread=%d, s='%s'\n", nread, s);
    return s;
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
    theErrNo = 0;
    int ret;
    if (!fp) { theErrNo = EINVAL; return EOF; }

    ret = sys_close(fp->file_fd);
    if (ret) {
        theErrNo = ret;
        return -1;
    }
    free(fp->file_buf);
    return 0;
}

int fflush(__unused FILE *stream) {
    logmsgef("fflush(fd=%d)\n", stream->file_fd);
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
    logmsgef("foef(fd=%d)\n", stream->file_fd);
    if (stream == stdin)
        return 0;
    return 1;
}

int rename(const char *old, const char *new) {
    theErrNo = 0;
    int ret = sys_rename(old, new);
    if (ret == 0)
        return 0;
    theErrNo = ret;
    return -1;
}

int remove(const char *path) {
    theErrNo = 0;
    int ret = sys_unlink(path);
    if (ret == 0)
        return 0;
    theErrNo = ret;
    return -1;
}

void clearerr(__unused FILE *stream) {
    logmsgdf("clearerr(fd=%d)\n", stream->file_fd);
    theErrNo = 0;
}

int ferror(__unused FILE *stream) {
    logmsgdf("ferror(fd=%d)\n", stream->file_fd);
    if (stream == stdin)  return 0;
    if (stream == stdout) return 0;
    if (stream == stderr) return 0;
    return 0; /* TODO */
}


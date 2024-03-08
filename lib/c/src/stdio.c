#include <stdio.h>

#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/errno.h>

// TODO: move to unistd wrappers
#include <sys/syscall.h>

#include <cosec/log.h>

extern int theErrNo;

struct FILE_struct {
    int file_fd;

    off_t       file_pos;
    off_t       file_size;
    unsigned    file_flags;
    char        file_ungetc;
    enum buffering_mode_t file_bufmode;

    size_t file_bufpos;
    size_t file_bufend;
    size_t file_bufsz;
    char * file_buf;
};

static inline void fstream_clearbuf(FILE *f) {
    if (f->file_bufmode == _IONBF)
        return;

    f->file_ungetc = 0;
    f->file_bufpos = f->file_bufend = 0;
}

static inline size_t fstream_pending(FILE *f) {
    if (f->file_bufmode == _IONBF)
        return 0;
    return f->file_bufend - f->file_bufpos;
}

char stdinbuf[BUFSIZ];
char stdoutbuf[BUFSIZ];

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


char * snprint_uint(char *str, char *const end, uint x, uint8_t base, uint flags, int precision, int minwidth) {
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

    if (~(flags & LEFT_PADDED)) {
        int indent = maxDigits - n;
        while (minwidth > indent) {
            --n;
            a[n] = (flags & ZERO_PADDED ? '0' : ' ');
            --minwidth;
        }
    }

    while (n < maxDigits) {
        if (end && (end <= str)) return end;
        *(str++) = a[n++];
    }

    /*
    if (flags & LEFT_PADDED) {
        while (precision > (maxDigits - n)) {
            *(str++) = ' ';
            --precision;
        }
    }
    */

    return str;
}

char * snprint_int(char *str, char *const end, int x, uint8_t base, uint flags, int precision, int minwidth) {
    if (end && (end <= str)) return end;

    if (x < 0) {
        *(str++) = '-';
        x = -x;
    } else if (flags & PRINT_PLUS)
        *(str++) = '+';
    else if (flags & SPACE_PLUS)
        *(str++) = ' ';

    return snprint_uint(str, end, x, base, flags, precision, minwidth);
}

const char * sscan_uint(const char *str, uint *res, const uint8_t base) {
    uint r = 0;
    const char *s = str;

    do {
        char c = toupper(*s);
        if (('0' <= c) && (c <= '9')) {
            r *= base;
            r += (c - '0');
        } else if ((base > 10) && ('A' <= c) && (c <= ('A' + base - 10))) {
            r *= base;
            r += (c - 'A' + 10);
        } else {
            if (s != str)
                *res = r;
            return s;
        }
        ++s;
    } while (1);
}

const char * sscan_int(const char *str, int *res, const uint8_t base) {
    char sign = 1;

    switch (*str) {
    case '-': sign = -1;
    case '+': ++str;
    }

    const char *end = sscan_uint(str, (uint *)res, base);
    if ((end != str) && (sign == -1))
        *res = - *res;
    return end;
}

int puts(const char *str) {
    return fputs(str, stdout);
}

int lprintf(const char *fmt, ...) {
    int ret;
    va_list ap;
    va_start(ap, fmt);
    ret = vlprintf(fmt, ap);
    va_end(ap);
    return ret;
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
    logmsgdf("%s(fd=%d, '%s', ...)\n", __func__, stream->file_fd, format);
    int ret = 0;

    /* TODO: this is so very ugly, make this better */
    int bufsize = 64;
    char *buf = malloc(bufsize);
    while (1) {
        ret = vsnprintf(buf, bufsize, format, ap);
        if (ret + 1 < bufsize)
            break;

        bufsize *= 2;
        buf = realloc(buf, bufsize);
    }

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

        if (fmt_c[0] != '%') {
            *out_c++ = *fmt_c++;
            continue;
        }

        ++fmt_c;  // '%'

        int precision = 0;
        int minwidth = 0;
        uint flags = 0;

        bool changed;
        do {
            changed = true;
            switch (*fmt_c) {
            case '0':   flags |= ZERO_PADDED; ++fmt_c; break;
            case '+':   flags |= PRINT_PLUS; ++fmt_c; break;
            case ' ':   flags |= SPACE_PLUS; ++fmt_c; break;
            default:
                changed = false;
            }
        } while (changed);

        if (isdigit(*fmt_c)) {
            fmt_c = sscan_uint(fmt_c, &minwidth, 10);
        }
        if (*fmt_c == '.') {
            fmt_c = sscan_int(++fmt_c, &precision, 10);
        }
        if (*fmt_c == 'l') {
            ++fmt_c;  /* TODO: handle 'l' */
        }

        switch (*fmt_c) {
        case 'x': {
            int arg = va_arg(ap, int);
            out_c = snprint_uint(out_c, end, arg, 16, flags, precision, minwidth);
            } break;
        case 'd': case 'i': {
            int arg = va_arg(ap, int);
            out_c = snprint_int(out_c, end, arg, 10, flags, precision, minwidth);
            } break;
        case 'u': {
            uint arg = va_arg(ap, uint);
            out_c = snprint_uint(out_c, end, arg, 10, flags, precision, minwidth);
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
            out_c = snprint_uint(out_c, end, intpart, 10, flags, 0, 0);
            if (fracpart > 0) {
                *out_c++ = '.';
                out_c = snprint_uint(out_c, end, fracpart, 10,
                                 ZERO_PADDED | LEFT_PADDED, 6, minwidth);
            }
            } break;
        case 'p': {
            void *arg = va_arg(ap, void *);
            *out_c++ = '0'; *out_c++ = 'x';
            out_c = snprint_uint(out_c, end, (uint)arg, 16,
                                 ZERO_PADDED | flags, 8, minwidth);
            } break;
        case '%':
            *out_c = *fmt_c;
            ++out_c;
            break;
        default:
            logmsgef("vsnprintf: unknown format specifier 0x%x at '%s'",
                     (uint)*fmt_c, fmt_c);
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

int fileno(FILE *stream) {
    if (!stream) { theErrNo = EINVAL; return -1; }
    logmsgdf("fileno(fd=%d)\n", stream->file_fd);
    return stream->file_fd;
}

static int fopen_on(const char *path, const char *mode, FILE *f) {
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
        return -fd;
    }

    f->file_flags = flags;
    f->file_pos = 0;
    off_t filesize = sys_lseek(fd, 0, SEEK_END);
    if (filesize < 0)
        f->file_pos = -1;
    else if (flags & O_APPEND) {
        f->file_pos = filesize;
    } else {
        sys_lseek(fd, 0, SEEK_SET);
    }
    f->file_size = filesize;
    f->file_ungetc = 0;

    /* init buffering */
    f->file_buf = malloc(BUFSIZ);
    f->file_bufmode = f->file_buf ? _IOFBF : _IONBF;
    f->file_bufsz = f->file_buf ? BUFSIZ : 0;
    f->file_bufpos = 0;
    f->file_bufend = 0;

    f->file_fd = fd;
    return 0;
}

FILE * fopen(const char *path, const char *mode) {
    logmsgdf("fopen('%s', '%s')\n", path, mode);
    theErrNo = 0;

    FILE *f = malloc(sizeof(FILE));
    if (!f) return NULL;

    int ret = fopen_on(path, mode, f);
    logmsgdf("fopen(%s): fd=%d, pos=%d, ret='%s'\n",
             path, f->file_fd, (int)f->file_pos, strerror(ret));
    if (ret) {
        theErrNo = ret;
        free(f);
        return NULL;
    }

    return f;
}

FILE *freopen(const char *path, const char *mode, FILE *stream) {
    if (stream) {
        fclose(stream);
    }

    int ret = fopen_on(path, mode, stream);
    if (ret) {
        theErrNo = ret;
        free(stream);
        return NULL;
    }
    return stream;
}

char *tmpnam(char *s) {
    logmsge("TODO: tmpnam('%s')", s);
    return "";
}


static size_t fread_nobuf(char *ptr, size_t nbytes, FILE *f) {
    if (nbytes < 1)
        return 0;

    if (f->file_ungetc) {
        ptr[0] = f->file_ungetc;
        f->file_ungetc = 0;
        ++ptr; --nbytes;
    }

    int ret = sys_read(f->file_fd, ptr, nbytes);
    if (ret < 0) {
        theErrNo = -ret;
        return 0;
    }

    f->file_pos += ret;
    return (size_t)ret;
}

static size_t fread_upto(char *ptr, size_t nbytes, FILE *f, char *upto) {
    const char *funcname = __FUNCTION__;
    char *buf = f->file_buf + f->file_bufpos;
    char *stop = NULL;
    size_t to_read;
    size_t nread = 0;

    if (nbytes < 1)
        return 0;

    if (f->file_ungetc) {
        ptr[0] = f->file_ungetc;
        f->file_ungetc = 0;
        ++ptr; --nbytes;
    }

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

    if (stop) goto fun_exit;

    while (nread < nbytes) {
        buf = f->file_buf;
        int sysread = sys_read(f->file_fd, buf, f->file_bufsz);
        logmsgdf("%s: sys_read(%d, *%x, %d) -> %d\n",
                funcname, f->file_fd, (uint)buf, f->file_bufsz, sysread);
        if (sysread <= 0) {
            theErrNo = -sysread;
            goto fun_exit;
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

        if (stop) goto fun_exit;
    }

fun_exit:
    f->file_pos += nread;
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

    if (stream->file_flags & O_WRONLY) {
        theErrNo = EBADF;
        return 0;
    }

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
    theErrNo = 0;
    logmsgdf("fwrite(*%x, %d, %d, fd=%d\n",
            (uint)ptr, size, nmemb, stream->file_fd);

    if (stream->file_flags & O_RDONLY) {
        theErrNo = EBADF;
        return 0;
    }

    /* TODO: sync buffer */
    int ret = sys_write(stream->file_fd, ptr, size * nmemb);
    if (ret >= 0) {
        theErrNo = -ret;
        return 0;
    }

    stream->file_pos += ret;
    return (size_t)ret;
}

int fgetc(FILE *f) {
    logmsgdf("fgetc(fd=%d)\n", f->file_fd);
    int ret = EOF;
    char c = 0;

    theErrNo = 0;
    if (!f) return EOF;

    if (f->file_flags & O_WRONLY) {
        theErrNo = EBADF; return -1;
    }
    if (f->file_ungetc) {
        ret = f->file_ungetc;
        f->file_ungetc = 0;
        return ret;
    }

    switch (f->file_bufmode) {
      case _IONBF:
        fread_nobuf(&c, 1, f);
        break;
      case _IOLBF: case _IOFBF:
        fread_upto(&c, 1, f, NULL);
        break;
      default:
        logmsgef("fgetc(fd=%d): unknown bufmode %d\n",
                f->file_fd, f->file_bufmode);
    }
    if (c) ret = (int)c;
    return ret;
}

int ungetc(int c, FILE *stream) {
    logmsgdf("ungetc(c=0x%x, fd=%d)\n", c, stream->file_fd);
    theErrNo = 0;

    if (stream->file_ungetc)
        return EOF;
    stream->file_ungetc = c;
    return c;
}

char *fgets(char *s, int size, FILE *stream) {
    theErrNo = 0;
    logmsgdf("fgets(*%x, %d, fd=%d)\n", (uint)s, size, stream->file_fd);
    if (stream->file_flags & O_WRONLY) {
        theErrNo = EBADF; return NULL;
    }

    char nl = '\n';
    size_t nread = fread_upto(s, size - 1, stream, &nl);
    if (nread < 1)
        return NULL;

    s[nread] = 0;
    logmsgdf("fgets: nread=%d, s='%s'\n", nread, s);
    return s;
}

int fputc(int ch, FILE *stream) {
    fwrite((const char *)&ch, 1, 1, stream);
    if (theErrNo != 0) {
        return EOF;
    }
    return ch;
}

int fputs(const char *str, FILE *stream) {
    size_t len = strlen(str);
    size_t nwritten = fwrite(str, len, 1, stream);
    if (theErrNo != 0) {
        return EOF;
    }
    if (fputc('\n', stream) != '\n') {
        return EOF;
    }
    return nwritten + 1;
}

long ftell(FILE *stream) {
    theErrNo = 0;
    if (!stream) { theErrNo = EINVAL; return -1; }

    logmsgdf("ftell(fd=%d) -> %d\n", stream->file_fd, stream->file_pos);
    return stream->file_pos;
}

int fseek(FILE *stream, long offset, int whence) {
    logmsgdf("fseek(fd=%d, off=%d, %s)\n", stream->file_fd, offset,
                (whence == SEEK_SET ? "SET" :
                    (whence == SEEK_CUR ? "CUR" :
                        (whence == SEEK_END ? "END" : "???"))));
    if (!stream) { theErrNo = EINVAL; return -1; }
    theErrNo = 0;
    int ret;

    switch (whence) {
      case SEEK_CUR:
        if (offset == 0) {
            ret = stream->file_pos;
            logmsgdf("fseek: file_pos=%d\n", ret);
            return 0;
        }
        offset += stream->file_pos;
        whence = SEEK_SET;
        break;
      case SEEK_SET: case SEEK_END:
        break;
      default: /* avoid syscall if EINVAL */
        logmsgf("fseek(whence=%d)\n", whence);
        theErrNo = EINVAL;
        return -1;
    }
    /* invalidate buffering */
    fstream_clearbuf(stream); /* TODO: try to optimize it */

    ret = sys_lseek(stream->file_fd, (off_t)offset, whence);
    if (ret < 0) {
        theErrNo = -ret;
        return -1;
    }

    stream->file_pos = ret;
    logmsgdf("fseek -> %d\n", ret);
    return 0;
}

int fclose(FILE *fp) {
    theErrNo = 0;
    int ret;
    if (!fp) { theErrNo = EINVAL; return EOF; }

    ret = sys_close(fp->file_fd);
    free(fp->file_buf);
    free(fp); /* TODO: freopen is broken due to this */

    if (ret) { theErrNo = ret; return EOF; }
    return 0;
}

int fflush(FILE *stream) {
    theErrNo = 0;
    logmsgdf("fflush(fd=%d)\n", stream->file_fd);
    if (!stream) {
        logmsgef("fflush(NULL): flush all open streams, TODO\n");
        return 0;
    }

    if (stream->file_bufmode == _IONBF)
        return 0;
    if (fstream_pending(stream) == 0)
        return 0;
    if (stream->file_flags & O_RDONLY) {
        /* discard buffer */
        fstream_clearbuf(stream);
    } else {
        /* write buffer */

        /* fwrite() writes immediately at the moment,
         * no need for fflush() to do anything
         */
    }
    return 0;
}

int setvbuf(FILE *stream, char *buf, int type, size_t size) {
    const char *funcname = __FUNCTION__;
    if (buf) {
        logmsgef("%s: setting a custom buffer is not supported, TODO", funcname);
        return ETODO;
    }
    if (0 > size || size > BUFSIZ) {
        logmsgef("%s: buffers more than %d are not supported, TODO", funcname, BUFSIZ);
        return ETODO;
    }
    switch (type) {
        case _IONBF:
            stream->file_bufmode = _IONBF;
            break;
        case _IOLBF:
            stream->file_bufmode = _IOLBF;
            break;
        case _IOFBF:
            stream->file_bufmode = _IOFBF;
            break;
        default:
            logmsgef("%s: unknown buffering type %d", funcname, type);
            return EINVAL;
    }
    stream->file_bufsz = size;
    return 0;
}

int feof(FILE *stream) {
    logmsgdf("feof(fd=%d)\n", stream->file_fd);
    if (stream->file_size < 0)
        return false;
    return (stream->file_pos == stream->file_size);
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

void clearerr(FILE *stream) {
    UNUSED(stream);
    logmsgdf("clearerr(fd=%d)\n", stream->file_fd);
    theErrNo = 0;
}

int ferror(FILE *stream) {
    logmsgdf("ferror(fd=%d)\n", stream->file_fd);
    if (stream == stdin)  return 0;
    if (stream == stdout) return 0;
    if (stream == stderr) return 0;
    return 0; /* TODO */
}


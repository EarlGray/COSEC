#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <limits.h>
#include <locale.h>
#include <setjmp.h>
#include <math.h>
#include <sys/errno.h>

#include <machine/setjmp.h>

#include <cosec/log.h>

#include <bits/libc.h>

// TODO: kernel/userspace ambiguity
extern void *krealloc(void *p, size_t size);

int theErrNo = 0;

const char * const sys_errlist[] = {
    [0] = "no error",
    [EPERM]   = "EPERM: invalied permissions",
    [ENOENT]  = "ENOENT: no such entity",
    [ESRCH]   = "ESRCH: no such process",
    [EINTR]   = "EINTR: interrupted system call",
    [EIO]     = "EIO: I/O error",
    [ENXIO]   = "ENXIO: No such device or address",
    [E2BIG]   = "E2BIG: Argument list too]long",
    [ENOEXEC] = "ENOEXEC: Exec format error",
    [EBADF]   = "EBADF: Bad file number",
    [ECHILD]  = "ECHILD: No child processes",
    [EAGAIN]  = "EAGAIN: Try again",
    [ENOMEM]  = "ENOMEM: Out of memory",
    [EACCES]  = "EACCES: Permission denied",
    [EFAULT]  = "EFAULT: Bad address",
    [ENOTBLK] = "ENOTBLK: Block device required",
    [EBUSY]   = "EBUSY: Device or resource busy",
    [EEXIST]  = "EEXIST: File exists",
    [EXDEV]   = "EXDEV: Cross-device link",
    [ENODEV]  = "ENODEV: No such device",
    [ENOTDIR] = "ENOTDIR: Not a directory",
    [EISDIR]  = "EISDIR: Is a directory",
    [EINVAL]  = "EINVAL: Invalid argument",
    [ENFILE]  = "ENFILE: File table overflow",
    [EMFILE]  = "EMFILE: Too many open files",
    [ENOTTY]  = "ENOTTY: Not a typewriter",
    [ETXTBSY] = "ETXTBSY: Text file busy",
    [EFBIG]   = "EFBIG: File too large",
    [ENOSPC]  = "ENOSPC: No space left on device",
    [ESPIPE]  = "ESPIPE: Illegal seek",
    [EROFS]   = "EROFS: Read-only file system",
    [EMLINK]  = "EMLINK: Too many links",
    [EPIPE]   = "EPIPE: Broken pipe",
    [EDOM]    = "EDOM: Math argument out of domain of fun",
    [ERANGE]  = "ERANGE: Math result not representable ",
    [ENOSYS]  = "ENOSYS: Functionality is not supported",

    [EKERN]   = "EKERN: Internal kernel error",
    [ETODO]   = "ETODO: Not implemented yet",
};

#define UKNERR_BUF_LEN 40
static char unknown_error[UKNERR_BUF_LEN] = "unknown error: ";
static size_t unknown_err_len = 0;

char const *strerror(int err) {
    if ((0 <= err) && ((size_t)err < sizeof(sys_errlist)/sizeof(size_t))) {
        const char *res = sys_errlist[err];
        logmsgdf("strerror: err=%d, *%x='%s'\n", err, (uint)res, res);
        if (res)
            return res;
    }

    logmsgdf("strerror: unknown error %d\n", err);

    if (!unknown_err_len)
        unknown_err_len = strlen(unknown_error);

    snprintf(unknown_error + unknown_err_len, UKNERR_BUF_LEN - unknown_err_len,
             "%d", err);
    return unknown_error;
}

int get_errno(void) {
    return theErrNo;
}

void perror(const char *s) {
    if (s && s[0]) {
        fprintf(stderr, "%s: %s\n", s, strerror(theErrNo));
    } else {
        fprintf(stderr, "%s\n", strerror(theErrNo));
    }
}

/* misc from stdlib.h */
int abs(int i) {
    if (i >= 0) return i;
    return -i;
}

int atoi(const char *nptr) {
    logmsgdf("atoi(%s)\n", nptr);
    while (isspace(*nptr)) ++nptr;

    int res = 0;
    int sign = 1;
    switch (*nptr) {
      case '+': nptr++; break;
      case '-': nptr++; sign = -1; break;
    }
    while (isdigit(*nptr)) {
        res *= 10;
        res += (*nptr) - '0';
        ++nptr;
    }
    return sign * res;
}

inline static int getbasedigit(char c, int base) {
    char lc = tolower(c);
    if ((base <= 0) || (base > 36))
        return -1;

    if (base > 10) {
        if (('a' <= lc) && (lc < ('a' + base - 10)))
            return (lc - 'a' + 10);
    }
    if (('0' <= lc) && (lc < ('0' + base)))
        return lc - '0';
    return -1;
}

long strtol(const char *nptr, char **endptr, int base) {
    logmsgdf("strtol('%s')\n", nptr);
    theErrNo = 0;
    char const *c = nptr;
    bool neg = false;
    long res = 0;

    if (base > 36)
        goto error_exit;

    while (isspace(*c)) ++c;

    switch (*c) {
      case '-': neg = true; /* fallthrough */
      case '+': ++c; break;
    }

    if (base == 0) {
        if (c[0] == '0') {
            ++c;
            if (tolower(*c) == 'x') {
                base = 16; ++c;
            } else if (getbasedigit(*c, 8) != -1) {
                base = 8;
            } else {
                if (endptr) *endptr = (char *)c;
                return 0;
            }
        }
        else base = 10;
    }

    if (getbasedigit(*c, base) == -1)
        goto error_exit;

    for (;;) {
        int digit = getbasedigit(*c, base);
        if (digit < 0) break;

        long oldres = res;
        res *= 10;
        res += digit;
        if (oldres > res) {
            theErrNo = ERANGE;
            if (endptr) *endptr = NULL;
            return 0;
        }
        ++c;
    }

    if (neg) res = -res;
    if (endptr) *endptr = (char *)c;
    return res;

error_exit:
    /* TODO: set EINVAL */
    if (endptr) *endptr = (char *)nptr;
    return 0;
}

double strtod(const char *nptr, char **endptr) {
    //logmsgef("strtod('%s')\n", nptr);
    theErrNo = 0;
    bool neg = false;
    const char *c = nptr;
    double res = 0;
    char decpoint = localeconv()->decimal_point[0];

    while (isspace(*c)) ++c;

    switch (*c) {
      case '-': neg = true; /* fallthrough */
      case '+': ++c; break;
    }

    if (!isdigit(*c) && (*c != decpoint))
        goto error_exit;

    /* TODO: hexadecimals, INF/INFINITY/NAN */
    while (isdigit(*c)) {
        res *= 10;
        res += (*c) - '0';
        ++c;
    }

    if (*c == decpoint) {
        double fracpow = 1;
        ++c;
        while (isdigit(*c)) {
            fracpow /= 10.0;
            res += fracpow * (*c - '0');
            ++c;
        }
    }

    if (tolower(*c) == 'e') {
        ++c;
        int expnum = 0;
        bool expneg = false;
        switch (*c) {
            case '-': expneg = true; /*fallthough*/
            case '+': ++c; break;
        }
        if (!isdigit(*c))
            goto error_exit;

        do {
            expnum *= 10;
            expnum += (*c - '0');
        } while (isdigit(*++c));
        res *= pow(10.0, (expneg ? -expnum : expnum));
    }

    if (endptr) *endptr = (char *)c;
    return (neg ? -res : res);

error_exit:
    theErrNo = EINVAL;
    if (endptr) *endptr = (char *)nptr;
    return 0;
}

/*
 * stdlib-like memory management using the global heap: stdlib.h
 */
void *malloc(size_t size) {
    theErrNo = 0;
    void *p = kmalloc(size);
    if (!p) theErrNo = ENOMEM;
    return p;
}

void *calloc(size_t nmemb, size_t size) {
    size_t sz = nmemb * size;
    char *p = malloc(sz);
    memset(p, 0x00, sz);
    return p;
}

void *realloc(void *ptr, size_t size) {
    theErrNo = 0;
    void *p = krealloc(ptr, size);
    if (!p) theErrNo = ENOMEM;
    return p;
}

void free(void *ptr) {
    kfree(ptr);
}

/*
 *  string operations from string.h
 */
int strncmp(const char *s1, const char *s2, size_t n) {
    //logmsgdf("strcmp(*%x, *%x, %x)\n", (size_t)s1, (size_t)s2, n);
    if (s1 == s2) return 0;
    size_t i = 0;
    while (i++ < n) {
        if ((*s1) != (*s2)) return ((*s1) - (*s2));
        if (0 == (*s1)) return 0;
        ++s1, ++s2;
    }
    return 0;
}

int strcoll(const char *s1, const char *s2) {
    logmsgd("TODO: strcoll defaults to strcmp()");
    return strcmp(s1, s2);
}

int strcmp(const char *s1, const char *s2) {
    //logmsgdf("strcmp(*%x, *%x)\n", (size_t)s1, (size_t)s2);
    return strncmp(s1, s2, UINT_MAX);
}

int strncasecmp(const char *s1, const char *s2, size_t n) {
    if (s1 == s2) return 0;
    size_t i = 0;
    while (i++ < n) {
        int lc1 = tolower(*s1);
        int lc2 = tolower(*s2);
        if (lc1 != lc2) return (lc2 - lc1);
        if (0 == (*s1)) return 0;
        ++s1, ++s2;
    }
    return 0;
}

int strcasecmp(const char *s1, const char *s2) {
    return strncasecmp(s1, s2, UINT_MAX);
}

size_t strspn(const char *s1, const char *s2) {
    char accept[UCHAR_MAX + 1] = { 0 };
    const char *s;
    for (s = s2; *s; ++s) {
        accept[ (uint8_t)*s ] = true;
    }

    s = s1;
    for (s = s1; *s; ++s) {
        if (!accept[ (uint8_t)*s ])
            return (size_t)(s - s1);
    }
    return (size_t)(s - s1);
}

int memcmp(const void *s1, const void *s2, size_t n) {
    index_t i;
    const char *c1 = s1;
    const char *c2 = s2;
    for (i = 0; i < n; ++i) {
        char diff = *c1 - *c2;
        if (diff)
            return diff;
        ++c2; ++c1;
    }
    return 0;
}

char *strndup(const char *s, size_t n) {
    if (s == null) return null;
    size_t len = strlen(s) + 1;
    if (len > n) len = n;
    char *d = (char *) kmalloc(len);
    if (!d) logmsgef("strndup: kmalloc failed!\n");
    return strncpy(d, s, n);
}

char *strdup(const char *s) {
    return strndup(s, UINT_MAX);
}

char *strcpy(char *dest, const char *src) {
    if ((src == null) || (dest == null))
        return null;
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    index_t i = 0;
    char *d = dest;
    while ((*src) && (i++ < n))
        *d++ = *src++;
    *d = '\0';
    /* while (i++ < n)  *d++ = '\0'; // Sorry: POSIX requires this */
    return dest;
}

size_t strlen(const char *s) {
    const char *c = s;
    while (*c) ++c;
    return c - s;
}

size_t strnlen(const char *s, size_t maxlen) {
    const char *c = s;
    while (*c && ((size_t)(c - s) < maxlen))
        ++c;
    return (size_t)(c - s);
}


static inline void *memcpy_from_start(uint8_t *dest, const uint8_t *src, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        dest[i] = src[i];
    }
    return dest;
}

static inline void *memcpy_from_end(uint8_t *dest, const uint8_t *src, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        dest[size - 1 - i] = src[size - 1 - i];
    }
    return dest;
}

void* memcpy(void *dest, const void *src, size_t size) {
    if (dest == src)
        return dest;
    return memcpy_from_start(dest, src, size);
}

void* memmove(void *dst, const void *src, size_t len) {
    if (dst == src)
        return dst;

    if ((src < dst) && (dst < src+len)) {
        /* an overlapping case that requires backward copying */
        return memcpy_from_end(dst, src, len);
    }
    return memcpy_from_start(dst, src, len);
}

void *memset(void *s, int c, size_t n) {
    char *p = (char *)s;
    index_t i;
    for (i = 0; i < n; ++i)
        p[i] = c;
    return s;
}

void *memchr(const void *s, int c, size_t n) {
    size_t i;
    const char *sp = s;
    for (i = 0; i < n; ++i) {
        if (sp[i] == c)
            return (void *)(s + i);
    }
    return NULL;
}

char *strnchr(const char *s, size_t n, int c) {
    char *cur = (char *)s;
    while (*cur && ((cur - s) < (int)n)) {
        if ((char)c == *cur)
            return cur;
        ++cur;
    }
    return null;
}

char *strnrchr(const char *s, size_t n, int c) {
    char *cur = (char *)s;
    char *last = null;
    while (*cur && ((size_t)(cur - s) < n)) {
        if ((char)c == *cur)
            last = cur;
        ++cur;
    }
    return last;
}

char *strchr(const char *s, int c) {
    return strnchr(s, INT_MAX, c);
}

char *strrchr(const char *s, int c) {
    return strnrchr(s, INT_MAX, c);
}

char *strstr(const char *haystack, const char *needle) {
    logmsge("TODO: strstr()");
    return NULL;
}

char *strpbrk(const char *s, const char *accept) {
    char accept_table[ UCHAR_MAX + 1] = { 0 };
    char *c;

    for (c = (char *)accept; *c; ++c)
        accept_table[(int)*c] = true;

    for (c = (char *)s; *c; ++c)
        if (accept_table[(unsigned)*c])
            return c;

    return NULL;
}

char *theStrtok = NULL;

char *strtok(char *str, const char *delim) {
    return strtok_r(str, delim, &theStrtok);
}

char *strtok_r(char *str, const char *delim, char **saveptr) {
    if (str == NULL) {
        str = *saveptr;
    }
    if (str == NULL) {
        return NULL;
    }

    /* skip all delimiters */
    while (str[0] && strchr(delim, str[0])) {
        ++str;
    }
    if (str[0] == '\0') {
        return NULL;
    }
    *saveptr = str;    // the start of the token

    /* consume the token */
    while (str[0] && !strchr(delim, str[0])) {
        ++str;
    }
    if (str[0] == '\0') {
        char *tok = *saveptr;
        *saveptr = NULL;  // no more tokens
        return tok;
    }

    str[0] = '\0';
    char *tok = *saveptr;
    *saveptr = str + 1;
    return tok;
}

/* Source: http://en.wikipedia.org/wiki/Jenkins_hash_function */
static uint32_t jenkins_one_at_a_time_hash(const char *key, size_t len) {
    uint32_t hash, i;
    for (hash = i = 0; i < len; ++i) {
        hash += key[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash += (hash >> 11);
    hash += (hash << 15);
    return hash;
}

uint32_t strhash(const char *key, size_t len) {
    return jenkins_one_at_a_time_hash(key, len);
}


/*
 *  char operations from ctype.h
 */
enum chargroups {
    CHARGRP_ALNUM = 0x0001,
    CHARGRP_ALPHA = 0x0002,
    CHARGRP_CNTRL = 0x0004,
    CHARGRP_DIGIT = 0x0008,
    CHARGRP_GRAPH = 0x0010,
    CHARGRP_HEXDI = 0x0020,
    CHARGRP_LOWER = 0x0040,
    CHARGRP_NUM   = 0x0080,
    CHARGRP_SPEC  = 0x0100,
    CHARGRP_PRINT = 0x0200,
    CHARGRP_PUNCT = 0x0400,
    CHARGRP_SPACE = 0x0800,
    CHARGRP_UPPER = 0x1000,
    CHARGRP_XDIGIT = 0x2000,
};

const uint16_t ascii_chrgrp[128] = {
    /* 00 */ 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
    /* 08 */ 0x0004, 0x0804, 0x0804, 0x0804, 0x0804, 0x0804, 0x0004, 0x0004,
    /* 10 */ 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
    /* 18 */ 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
    /* 20 */ 0x0800, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    /* 28 */ 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    /* 30 */ 0x20b9, 0x20b9, 0x20b9, 0x20b9, 0x20b9, 0x20b9, 0x20b9, 0x20b9,
    /* 38 */ 0x20b9, 0x20b9, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    /* 40 */ 0x0010, 0x3033, 0x3033, 0x3033, 0x3033, 0x3033, 0x3033, 0x1013,
    /* 48 */ 0x1013, 0x1013, 0x1013, 0x1013, 0x1013, 0x1013, 0x1013, 0x1013,
    /* 50 */ 0x1013, 0x1013, 0x1013, 0x1013, 0x1013, 0x1013, 0x1013, 0x1013,
    /* 58 */ 0x1013, 0x1013, 0x1013, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    /* 60 */ 0x0010, 0x2073, 0x2073, 0x2073, 0x2073, 0x2073, 0x2073, 0x0053,
    /* 68 */ 0x0053, 0x0053, 0x0053, 0x0053, 0x0053, 0x0053, 0x0053, 0x0053,
    /* 70 */ 0x0053, 0x0053, 0x0053, 0x0053, 0x0053, 0x0053, 0x0053, 0x0053,
    /* 78 */ 0x0053, 0x0053, 0x0053, 0x0010, 0x0010, 0x0010, 0x0010, 0x0004,
};

int isalnum(int c) { return (ascii_chrgrp[c] & CHARGRP_ALNUM); }
int isalpha(int c) { return (ascii_chrgrp[c] & CHARGRP_ALPHA); }
int isspace(int c) { return (ascii_chrgrp[c] & CHARGRP_SPACE); }
int isdigit(int c) { return (ascii_chrgrp[c] & CHARGRP_DIGIT); }

int islower(int c) { return (ascii_chrgrp[c] & CHARGRP_LOWER); }
int isupper(int c) { return (ascii_chrgrp[c] & CHARGRP_UPPER); }

int iscntrl(int c) { return (ascii_chrgrp[c] & CHARGRP_CNTRL); }
int isgraph(int c) { return (ascii_chrgrp[c] & CHARGRP_GRAPH); }
int ispunct(int c) { return (ascii_chrgrp[c] & CHARGRP_PUNCT); }
int isxdigit(int c) { return (ascii_chrgrp[c] & CHARGRP_XDIGIT); }

int isprint(int c) { return (ascii_chrgrp[c] & CHARGRP_PRINT); }

int tolower(int c) {
    if (('A' <= c) && (c <= 'Z')) return c + 'a' - 'A';
    return c;
}

int toupper(int c) {
    if (('a' <= c) && (c <= 'z')) return c + 'A' - 'a';
    return c;
}


/*
 *  Locale stuff
 */
struct lconv clocale = {
    .decimal_point = ".",
    .thousands_sep = "",
    .grouping      = "",
    .int_curr_symbol = "",
    .currency_symbol = "",
    .mon_decimal_point = "",
    .mon_thousands_sep = "",
    .mon_grouping      = "",
    .positive_sign     = "",
    .negative_sign     = "",
    .int_frac_digits   = '\x7f',
    .frac_digits       = '\x7f',
    .p_cs_precedes     = '\x7f',
    .p_sep_by_space    = '\x7f',
    .n_cs_precedes     = '\x7f',
    .n_sep_by_space    = '\x7f',
    .p_sign_posn    = '\x7f',
    .n_sign_posn    = '\x7f',
};

struct lconv *localeconv(void) {
    return &clocale;
}

char *setlocale(int category, const char *locale) {
    logmsgf("setlocale(%s)\n", locale);
    return "C";
}

/*
 *  Environment
 */
char **environ;

char *getenv(const char *name) {
#if COSEC_KERN
    if (!strcmp(name, "UNAME")) {
        return "COSEC";
    }
#endif
    if (!environ) {
        logmsgf("TODO: getenv('%s')\n", name);
        return NULL;
    }

    size_t namelen = strlen(name);
    for (char **pair = environ; *pair; ++pair) {
        if (strncmp(name, *pair, namelen) == 0 && (*pair)[namelen] == '=') {
            return *pair + namelen + 1;
        }
    }
    return NULL;
}


/*
 *  System control flow
 *  setjmp/longjmp
 */

void __stack_chk_fail(void) {
    panic("__stack_chk_fail()");
}


int system(const char *command) {
    logmsgef("TODO: system('%s')", command);
    return -ETODO;
}


int setjmp(jmp_buf env) {
    return i386_setjmp(env);
}

void longjmp(jmp_buf env, int val) {
    i386_longjmp(env, val);
}

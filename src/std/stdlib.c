#include <stdlib.h>

#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <limits.h>
#include <locale.h>

#include <arch/i386.h>
#include <mem/kheap.h>

#include <log.h>

/* misc from stdlib.h */
int abs(int i) {
    if (i >= 0) return i;
    return -i;
}

int atoi(const char *nptr) {
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

/*
long strtol(const char *nptr, char **endptr, int base) {
    const char *c = nptr;
}
*/

double strtod(const char *nptr, char **endptr) {
    bool neg = false;
    char *c = nptr;
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
        res *= pow(10,0, (expneg ? -expnum : expnum));
    }

    if (endptr) *endptr = c;
    return (neg ? -res : res);

error_exit:
    if (endptr) *endptr = nptr;
    return 0;
}

/*
 * stdlib-like memory management using the global heap: stdlib.h
 */
void *malloc(size_t size) {
    return kmalloc(size);
}

void *calloc(size_t nmemb, size_t size) {
    size_t sz = nmemb * size;
    char *p = kmalloc(sz);
    memset(p, 0x00, sz);
    return p;
}

void free(void *ptr) {
    kfree(ptr);
}

/*
 *  string operations from string.h
 */
int __pure strncmp(const char *s1, const char *s2, size_t n) {
    if (s1 == s2) return 0;
    size_t i = 0;
    while (i++ < n) {
        if ((*s1) != (*s2)) return ((*s2) - (*s1));
        if (0 == (*s1)) return 0;
        ++s1, ++s2;
    }
    return 0;
}

int __pure strcoll(const char *s1, const char *s2) {
    return strcmp(s1, s2);
}

int __pure strcmp(const char *s1, const char *s2) {
    return strncmp(s1, s2, UINT_MAX);
}

int __pure strncasecmp(const char *s1, const char *s2, size_t n) {
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

int __pure strcasecmp(const char *s1, const char *s2) {
    return strncasecmp(s1, s2, UINT_MAX);
}

int __pure memcmp(const void *s1, const void *s2, size_t n) {
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

size_t __pure strlen(const char *s) {
    const char *c = s;
    while (*c) ++c;
    return c - s;
}

size_t __pure strnlen(const char *s, size_t maxlen) {
    const char *c = s;
    index_t i = 0;
    while ((i++ < maxlen) && *c) ++c;
    return c - s;
}

void* memcpy(void *dest, const void *src, size_t size) {
    //arch_memcpy(dest, src, size);
    index_t i = 0;
    uint8_t *d = dest;
    const uint8_t *s = src;
    while (i++ < size)
        *d++ = *s++;
    return dest;
}

void *memset(void *s, int c, size_t n) {
    char *p = (char *)s;
    index_t i;
    for (i = 0; i < n; ++i)
        p[i] = c;
    return s;
}

char __pure *strnchr(const char *s, size_t n, int c) {
    char *cur = (char *)s;
    while (*cur && ((cur - s) < (int)n)) {
        if ((char)c == *cur)
            return cur;
        ++cur;
    }
    return null;
}

char __pure *strnrchr(const char *s, size_t n, int c) {
    char *cur = (char *)s;
    char *last = null;
    while (*cur && ((cur - s) < (int)n)) {
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
}

char *strpbrk(const char *s, const char *accept) {
    char accept_table[ UCHAR_MAX + 1] = { 0 };
    char *c;

    for (c = accept; *c; ++c)
        accept_table[*c] = true;

    for (c = s; *c; ++c)
        if (accept_table[*c])
            return c;

    return NULL;
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

char *strerror(int errornum) {
    return "strerror()";
}


/*
 *  char operations from ctype.h
 */
int tolower(int c) {
    if (('A' <= c) && (c <= 'Z')) return c + 'a' - 'A';
    return c;
}

int toupper(int c) {
    if (('a' <= c) && (c <= 'z')) return c + 'A' - 'a';
    return c;
}

int isspace(int c) {
    switch (c) {
      case ' ': case '\t':      /* see 'man isspace' */
      case '\n': case '\r':
      case '\v': case '\f': return true;
      default: return false;
    }
}

int isdigit(int c) {
    if ('0' <= c && c <= '9') return true;
    return false;
}


void __stack_chk_fail(void) {
    panic("__stack_chk_fail()");
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
    logmsgf("Tried to change locale to %s\n", locale);
    return "C";
}

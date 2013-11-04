#include <std/string.h>
#include <std/ctype.h>
#include <arch/i386.h>
#include <mem/kheap.h>

/* misc from stdlib.h */
int atoi(const char *nptr) {
    int res = 0;
    while (isdigit(nptr)) {
        res *= 10;
        res += (*nptr) - '0';
    }
    return res;
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

int __pure strcmp(const char *s1, const char *s2) {
    return strncmp(s1, s2, MAX_UINT);
}

int __pure strncasecmp(const char *s1, const char *s2, size_t n) {
    if (s1 == s2) return 0;
    size_t i = 0;
    while (i++ < n) {
        if (tolower(*s1) != tolower(*s2)) return ((*s2) - (*s1));
        if (0 == (*s1)) return 0;
        ++s1, ++s2;
    }
    return 0;
}

int __pure strcasecmp(const char *s1, const char *s2) {
    return strncmp(s1, s2, MAX_UINT);
}

int __pure memcmp(const void *s1, const void *s2, size_t n) {
    int i;
    const char *c1 = s1;
    const char *c2 = s2;
    for (i = 0; i < (int)n; ++i) {
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
    return strncpy(d, s, n);
}

char *strdup(const char *s) {
    return strndup(s, MAX_UINT);
}

char *strcpy(char *dest, const char *src) {
    if ((src == null) || (dest == null))
        return null;
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i = 0;
    char *d = dest;
    while ((*src) && (i++ < n))
        *d++ = *src++;
    /*while (i++ < n)  *d++ = '\0'; // Sorry: POSIX requires this */
    return dest;
}

size_t __pure strlen(const char *s) {
    const char *c = s;
    while (*c) ++c;
    return c - s;
}

size_t __pure strnlen(const char *s, size_t maxlen) {
    const char *c = s;
    size_t i = 0;
    while ((i++ < maxlen) && *c) ++c;
    return c - s;
}

void* memcpy(void *dest, const void *src, size_t size) {
    //arch_memcpy(dest, src, size);
    size_t i = 0;
    uint8_t *d = dest;
    const uint8_t *s = src;
    while (i++ < size)
        *d++ = *s++;
    return dest;
}

void *memset(void *s, int c, size_t n) {
    char *p = (char *)s;
    unsigned i;
    for (i = 0; i < n; ++i)
        p[i] = c;
    return s;
}

char __pure *strnchr(char *s, size_t n, char c) {
    char *cur = (char *)s;
    while (*cur && ((cur - s) < (int)n)) {
        if (c == *cur)
            return cur;
        ++cur;
    }
    return null;
}

char __pure *strnrchr(char *s, size_t n, char c) {
    char *cur = (char *)s;
    char *last = null;
    while (*cur && ((cur - s) < (int)n)) {
        if (c == *cur)
            last = cur;
        ++cur;
    }
    return last;
}

char *strchr(char *s, char c) {
    return strnchr(s, MAX_INT, c);
}

char *strrchr(char *s, char c) {
    return strnrchr(s, MAX_INT, c);
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
      case ' ': case '\t': return true;
      default: return false;
    }
}

int isdigit(int c) {
    if ('0' <= c && c <= '9') return true;
    return false;
}

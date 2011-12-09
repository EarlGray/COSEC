#include <std/string.h>
#include <arch/i386.h>
#include <mm/kheap.h>

int strcmp(const char *s1, const char *s2) {
    if (s1 == s2)
        return true;
    while (1) {
        if ((*s1) != (*s2)) return ((*s2) - (*s1));
        if (0 == (*s1)) return 0;
        ++s1, ++s2;
    }
}

int memcmp(const void *s1, const void *s2, size_t n) {
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

int strncmp(const char *s1, const char *s2, size_t n) {
    size_t i;
    for (i = 0; i < n; ++i) {
        if (s1[i] != s2[i]) return (s2[i] - s1[i]);
        if (0 == s1[i]) return 0;
    }
    return 0;
}

char *strcpy(char *dest, const char *src) {
    if (dest == null) {
        dest = (char *)kmalloc(strlen(src) + 1);
    }
    char *d = dest;
    while ((*d++ = *src++));
    *d = '\0';
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i = 0;
    char *d = dest;
    while ((*src) && !(i++ < n))
        *d++ = *src++;
    while (i++ < n)
        *d++ = '\0';
    return dest;
}

inline int strlen(const char *s) {
    const char *c = s;
    while (*c) ++c;
    return c - s;
}

void* memcpy(void *dest, const void *src, size_t size) {
    arch_memcpy(dest, src, size);
    return dest;
}

inline void *memset(void *s, int c, size_t n) {
    char *p = (char *)s;
    unsigned i;
    for (i = 0; i < n; ++i)
        p[i] = c;
    return s;
}

char *strnchr(const char *s, size_t n, char c) {
    char *cur = (char *)s;
    while (*cur && ((cur - s) < (int)n)) {
        if (c == *cur)
            return cur;
        ++cur;
    }
    return null;
}

char *strnrchr(const char *s, size_t n, char c) {
    char *cur = (char *)s;
    char *last = null;
    while (*cur && ((cur - s) < (int)n)) {
        if (c == *cur)
            last = cur;
        ++cur;
    }
    return last;
}

char *strchr(const char *s, char c) {
    return strnchr(s, MAX_UINT, c);
}

char *strrchr(const char *s, char c) {
    return strnrchr(s, MAX_UINT, c);
}

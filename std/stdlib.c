#include <std/string.h>
#include <dev/cpu.h>

int strcmp(const char *s1, const char *s2) {
    while (1) {
        if ((*s1) != (*s2)) return ((*s2) - (*s1));   
        if (0 == (*s1)) return 0;
        ++s1, ++s2;
    } 
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
    arch_strncpy(dest, src, 0xFFFFFFFF);
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    arch_strncpy(dest, src, n);
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

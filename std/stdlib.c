#include <std/string.h>

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

inline int strlen(const char *s) {
    const char *c = s;
    while (*c) ++c;
    return c - s;
}

void* memcpy(void *dest, const void *src, size_t size) {
    asm("movl %0, %%edi \n" : : "r"(dest));
    asm("movl %0, %%esi \n" : : "r"(src));
    asm("movl %0, %%ecx \n" : : "r"(size));
    asm("rep movsb"); 
/*    int i;
    char *d = dest, *s = src;
    for (i = 0; i < size; ++i) 
        d[i] = s[i]; */

    return dest;
}

inline void *
memset(void *s, int c, size_t n) {
    char *p = (char *)s;
    unsigned i;
    for (i = 0; i < n; ++i)
        p[i] = c;
    return s;
}

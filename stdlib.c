#include <std/string.h>

int strcmp(const char *s1, const char *s2) {
    while (1) {
        if ((*s1) != (*s2)) return ((*s2) - (*s1));   
        if (0 == (*s1)) return 0;
        ++s1;
        ++s2;
    }
}

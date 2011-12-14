#ifndef __STRING_H__
#define __STRING_H__

int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);

char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);

int strlen(const char *s);

void* memset(void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void* memcpy(void *dest, const void *src, size_t n);

char *strchr(const char *s, char c);
char *strrchr(const char *s, char c);

char *strnchr(const char *s, size_t n, char c);
char *strnrchr(const char *s, size_t n, char c);

#endif //__STRING_H__

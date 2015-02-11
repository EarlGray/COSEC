#ifndef __STRING_H__
#define __STRING_H__

#include <stdint.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);

int strcoll(const char *s1, const char *s2);

int strncasecmp(const char *s1, const char *s2, size_t n);
int strcasecmp(const char *s1, const char *s2);

char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);

size_t strlen(const char *s);
size_t strnlen(const char *s, size_t maxlen);

void* memset(void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void* memcpy(void *dest, const void *src, size_t n);

char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);

void *memchr(const void *s, int c, size_t n);
void *memrchr(const void *s, int c, size_t n);

char *strstr(const char *haystack, const char *needle);
char *strcasestr(const char *haystack, const char *needle);

char *strndup(const char *s, size_t n);
char *strdup(const char *s);

char *strnchr(const char *s, size_t n, int c);
char *strnrchr(const char *s, size_t n, int c);

char *strpbrk(const char *s, const char *accept);
size_t strspn(const char *s, const char *accept);

char *strerror(int errornum);

uint32_t strhash(const char *key, size_t len);

#endif //__STRING_H__

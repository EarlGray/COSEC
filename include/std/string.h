#ifndef __STRING_H__
#define __STRING_H__

int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);

char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);

int strlen(const char *s);

#endif //__STRING_H__

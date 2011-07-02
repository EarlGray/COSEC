#ifndef __STDIO_H__
#define __STDIO_H__

#include <std/stdarg.h>

int getchar(void);
int putchar(int);

int printf(const char *format, ...);
//int fprintf(FILE *stream, const char *format, ...);
int sprintf(char *str, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);

#endif //__STDIO_H__

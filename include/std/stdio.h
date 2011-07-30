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

const char * sscan_uint(const char *str, uint *res, const uint8_t base);
const char * sscan_int(const char *str, int *res, const uint8_t base);

#endif //__STDIO_H__

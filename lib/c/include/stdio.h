#ifndef __STDIO_H__
#define __STDIO_H__

#include <stdarg.h>
#include <stdint.h>
#include <fcntl.h>

#define EOF     0xE0FE0F

enum buffering_mode_t {
    _IONBF, /* unbuffered */
    _IOFBF, /* full buffering */
    _IOLBF, /* line buffering */
};

typedef uint64_t fpos_t;

typedef struct FILE_struct  FILE;

extern FILE *stdin, *stdout, *stderr;

#define getc(stream) fgetc(stream)

FILE *fopen(const char *path, const char *mode);
FILE *freopen(const char *path, const char *mode, FILE *stream);

size_t fread(void *ptr, size_t size, size_t nmmeb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);

int fgetc(FILE *stream);
int ungetc(int c, FILE *stream);
char *fgets(char *s, int size, FILE *stream);

int printf(const char *format, ...);
int fprintf(FILE *stream, const char *format, ...);
int sprintf(char *str, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);

int vfprintf(FILE *stream, const char *format, va_list ap);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);

int fscanf(FILE *stream, const char *format, ...);

int fseek(FILE *stream, long offset, int whence);

long ftell(FILE *stream);
void rewind(FILE *stream);

int fgetpos(FILE *stream, fpos_t *pos);
int fsetpos(FILE *stream, fpos_t *pos);

int fflush(FILE *stream);

int feof(FILE *);
void clearerr(FILE *);
int ferror(FILE *stream);

int setvbuf(FILE *stream, char *buf, int mode, size_t size);

int fclose(FILE *);


char const *strerror(int err);

#define L_tmpnam  0
char *tmpnam(char *s);
FILE *tmpfile(void);

int remove(const char *pathname);
int rename(const char *oldpath, const char *newpath);

const char * sscan_uint(const char *str, uint *res, const uint8_t base);
const char * sscan_int(const char *str, int *res, const uint8_t base);

#endif //__STDIO_H__

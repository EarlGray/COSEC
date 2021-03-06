#ifndef __COSEC_STDLIB_H
#define __COSEC_STDLIB_H

#include <stdint.h>
#include <stddef.h>

# define UNUSED(v)   (void)v

#define NULL ((void *)0)

#define RAND_MAX    (0x7fffffff)

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);

void *realloc(void *ptr, size_t size);

void free(void *ptr);

int abs(int i);
int atoi(const char *nptr);
double strtod(const char *nptr, char **endptr);

long strtol(const char *nptr, char **endptr, int base);

int rand(void);
void srand(unsigned int seed);

enum exit_status { EXIT_SUCCESS = 0, EXIT_FAILURE = 1 };

char *getenv(const char *name);
int system(const char *command);

void abort(void);
void exit(int status);
void _Exit(int status);

int exitpoint(void);

#define tmalloc(_type)  ((_type *) kmalloc(sizeof(_type)))

#define null ((void *)0)
#define NULL ((void *)0)

#endif //__COSEC_STDLIB_H

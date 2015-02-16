#ifndef __COSEC_STDLIB_H
#define __COSEC_STDLIB_H

#include <stdint.h>
#include <stddef.h>
//#include <limits.h>

#define NULL ((void *)0)

#define RAND_MAX    (0x7fffffff)

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);

void *realloc(void *ptr, size_t size);

void free(void *ptr);


void *kmalloc(size_t);
int kfree(void *);

int abs(int i);
int atoi(const char *nptr);
double strtod(const char *nptr, char **endptr);

int rand(void);
void srand(unsigned int seed);

#define EXITENV_SUCCESS     -1
#define EXITENV_EXITPOINT   -2
#define EXITENV_ABORTED     -3

enum exit_status { EXIT_SUCCESS, EXIT_FAILURE };

char *getenv(const char *name);
int system(const char *command);

void abort(void);
void exit(int status);


#define tmalloc(_type)  ((_type *) kmalloc(sizeof(_type)))

#define null ((void *)0)
#define NULL ((void *)0)

#endif //__COSEC_STDLIB_H

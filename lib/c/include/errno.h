#ifndef __COSEC_ERRNO_H__
#define __COSEC_ERRNO_H__

#include <sys/errno.h>

int get_errno(void);
extern const char * const sys_errlist[];

#define errno get_errno()

#endif //__COSEC_ERRNO_H__

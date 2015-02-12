#ifndef __COSEC_SETJMP_H__
#define __COSEC_SETJMP_H__

//#include <arch/setjmp.h>

typedef int jmp_buf[];

int setjmp(jmp_buf env);

void longjmp(jmp_buf env, int val);

#endif // __COSEC_SETJMP_H__

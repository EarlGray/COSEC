#ifndef __COSEC_SETJMP_H__
#define __COSEC_SETJMP_H__

/**
 *  please keep it synchonized with the same value
 *  in arch/setjmp.h
 */
#define MY_JMPBUF_SIZE 6

typedef int jmp_buf[MY_JMPBUF_SIZE];

int setjmp(jmp_buf env);

void longjmp(jmp_buf env, int val);

#endif // __COSEC_SETJMP_H__

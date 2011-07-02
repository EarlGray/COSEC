#ifndef __STD_STDARG_H__
#define __STD_STDARG_H__

typedef struct {
    void *start, *current;
} va_list;

#define va_start(v,l)  \
{   (v).start = (void *)((char *)(l) + sizeof(l));  \
    (v).current = (v).start;    }

#define va_end(v) \
{   (v).current = (v).start;  }

#define va_arg(v,t) \
    (*((((t *)((v).current))) ++))
    
/*
#define va_arg(v, t) {  \
    t *arg = (t *)((v).current);    \
    (v).current = (void *)((char *)((v).current) + sizeof(t));    \
    return arg; }
    */

#endif //__STD_STDARG_H__

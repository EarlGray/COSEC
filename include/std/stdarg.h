#ifndef __STD_STDARG_H__
#define __STD_STDARG_H__

typedef struct {
    void *start;
    char *current;
} va_list;

#define va_start(v,l)  \
{   (v).start = (void *)((char *)&(l) + sizeof(l));  \
    (v).current = (v).start;    }

#define va_end(v) \
{   (v).current = (v).start;  }

#define va_aligned_size(type)   \
    (sizeof(type))

#define va_arg(arg, type)  \
    ( arg.current += va_aligned_size(type) , *(type *)(arg.current - va_aligned_size(type)))
    
#endif //__STD_STDARG_H__

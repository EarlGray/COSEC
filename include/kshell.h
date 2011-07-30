#ifndef __KSHELL_H__
#define __KSHELL_H__

void kshell_run(void);
void panic(const char *);

void print_mem(const char *p, size_t count);
void print_cpu(void);

#endif // __KSHELL_H__

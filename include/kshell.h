#ifndef __KSHELL_H__
#define __KSHELL_H__

void kshell_run(void);
void kpanic(const char *);

void print_mem(const char *p, size_t count);
void print_cpu(void);

void kshell_readline(char *buf, size_t size, const char *prompt);

#endif // __KSHELL_H__

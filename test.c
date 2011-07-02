#include <misc/test.h>
#include <std/string.h>
#include <std/stdarg.h>

const char * sscan_int(const char *str, int *res, uint8_t base);

static void test_sscan_once(const char *num) {
    int res;
    sscan_int(num, &res, 10);
    k_printf("sscan_int('%s') = %d\n", num, res);
}

static void test_sscan(void) {
    const char *sscan[] = {
        "12345", "1234adf2134", "-1232", "0", "+2.0", 0
    };
    int res;
    const char **iter = sscan;
    while (*iter) 
        test_sscan_once(*(iter++));
}

static void test_sprint(void) {
    char buf[200];
    sprintf(buf, "%s\n", "this is a string with %% and %n\n");
    k_printf(":\n%s\n", buf);

    sprintf(buf, "0x%x %u %i %d", 0xDEADBEEF, 42, -12345678, 0);
    k_printf(":\n%s\n", buf);

    sprintf(buf, "'%0.8x'\n'%+.8x'\n'%.8x'\n", 0xA, 0xFEAF, 0xC0FEF0CE);
    k_printf(":\n%s\n", buf);
}

static void test_vsprint(const char *fmt, ...) {
#define buf_size    200
    char buf[buf_size];
    va_list ap;
    va_start(ap, fmt);
//    vsnprintf(buf, buf_size, fmt, ap);

    char *c = fmt;
    print_mem(&fmt, 0x100);
    while (*c) {
        switch (*c) {
        case '%':
            switch (*(++c)) {
                case 's': {
                    char *s = va_arg(ap, char *); 
                    k_printf("%%s: 0x%x, *s=0x%x\n", (uint)s, (uint)*s);
                          } break;
                case 'd': case 'u': case 'x': {
                    int arg = va_arg(ap, int);
                    k_printf("%%d: 0x%x\n", (uint)arg);
                                              } break;
                default:
                    k_printf("Unknown operand for %%\n");
                        
            }
        }
        ++c;
    }

    va_end(ap);
    k_printf("\n%s\n", buf);
}

void test_sprintf(void) {
    test_vsprint("%s: %d, %s: %+x\n", "test1", -100, "test2", 200);
}

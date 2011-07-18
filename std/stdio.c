#include <dev/screen.h>
#include <dev/kbd.h>
#include <dev/cpu.h>

#include <std/stdarg.h>

volatile char c = 0;

static void on_key_press(scancode_t scan_code) {
    c = translate_from_scan(null, scan_code);
}

int getchar(void) {
    kbd_set_onpress(on_key_press);
    c = 0;
    while (c == 0) cpu_halt();
    kbd_set_onpress(null);
    return c;
}

int putchar(int c) {
    cprint((uint8_t)c);
    return c;
}

/***
  *     I/O
 ***/

#define ZERO_PADDED     0x0001
#define PRINT_PLUS      0x0002
#define LEFT_PADDED     0x0004
#define SPACE_PLUS      0x0008

#define get_digit(digit) \
    ( ((digit) < 10) ? ('0' + (digit)) : ('A' + (digit) - 10))

char * snprint_uint(char *str, char *const end, uint x, uint8_t base, uint flags, int precision) {
#define maxDigits 32	// 4 * 8 max for binary
    char a[maxDigits] = { 0 };
    uint8_t n = maxDigits;
    if (x == 0) {
        --n;
        a[n] = '0';
    } else 
    while (x != 0) {
        --n;
        a[n] = get_digit(x % base);
        x /= base;
    }

    if (~(flags & LEFT_PADDED)) {
        int indent = maxDigits - n;
        while (precision > indent) {
            --n;
            a[n] = (flags & ZERO_PADDED ? '0' : ' ');
            --precision;
        }
    } 

    while (n < maxDigits) {
        if (end && (end <= str)) return end;
        *(str++) = a[n++];
    }

    if (flags & LEFT_PADDED) {
        while (precision > (maxDigits - n)) {
            *(str++) = ' ';
            --precision;
        }
    }

    return str;
}

char * snprint_int(char *str, char *const end, int x, uint8_t base, uint flags, int precision) {
    if (end && (end <= str)) return end;

   	if (x < 0) { 
        *(str++) = '-'; 
        x = -x;
    } else if (flags & PRINT_PLUS) 
        *(str++) = '+'; 
    else if (flags & SPACE_PLUS) 
        *(str++) = ' ';

    return snprint_uint(str, end, x, base, flags, precision);
}

const char * sscan_int(const char *str, int *res, uint8_t base) {
    char sign = 1;
    bool read = true;

    switch (*str) {
    case '-': sign = -1;
    case '+': break;
    default: read = false;
    }

    *res = 0;
    do {
        if (read) ++str;
        read = true;
    
        char c = *str;
        if (('0' <= c) && (c <= '9')) {
            *res *= base;
            *res += (c - '0');
        } else
        if (('A' <= c) && (c <= ('A' + base - 10))) {
            *res *= base;
            *res += (c - 'A' + 10);
        } else
        if (('a' <= c) && (c <= ('a' + base - 10))) {
            *res *= base;
            *res += (c - 'a' + 10);
        } else 
            read = false;

    } while (read);

    if (sign == -1) 
        *res = - *res;
    return str;
}

int snprintf(char *str, size_t size, const char *format, ...) {
    void *stack = (void *)((void **)&format + 1);

    const char *fmt_c = format;
    char *out_c = str;
    char *end = (size == 0 ? 0 : str + size - 1);

    while (*fmt_c) {
        if (end && (out_c >= end)) {
            *end = 0;
            return end - str;
        }

        switch (*fmt_c) {
        case '%': {
            int precision = 0;
            uint flags = 0;
            ++fmt_c;

            bool changed;
            do {
                changed = true;
                switch (*fmt_c) {
                case '0':   flags |= ZERO_PADDED; ++fmt_c; break;
                case '+':   flags |= PRINT_PLUS; ++fmt_c; break;
                case ' ':   flags |= SPACE_PLUS; ++fmt_c; break;
                case '.':
                    fmt_c = sscan_int(++fmt_c, &precision, 10);
                    break;
                default:    
                    changed = false;
                }   
            } while (changed);

            switch (*fmt_c) {
            case 'x': {
                uint *arg = (uint *)stack;
                out_c = snprint_uint(out_c, end, *arg, 16, flags, precision);
                stack = (void *)(arg + 1);
                } break;
            case 'd': case 'i': {
                int *arg = (int *)stack;
                out_c = snprint_int(out_c, end, *arg, 10, flags, precision);
                stack = (void *)(arg + 1);
                } break;
            case 'u': {
                uint *arg = (uint *)stack;
                out_c = snprint_uint(out_c, end, *arg, 10, flags, precision);
                stack = (void *)(arg + 1);
                } break;
            case 's': {
                char *c = *(char **)stack;
                while (*c) {
                    if (end && (out_c >= end)) break;
                    *(out_c++) = *(c++);
                }

                stack = (void *)((char **)stack + 1);
                } break;
            //case 'e': case 'f': case 
            default:
                *out_c = *fmt_c;
                ++out_c;
            }
            } break;
        default:
            *out_c = *fmt_c;
            ++out_c;
        }
        ++fmt_c;
    }
    *out_c = 0;
    return out_c - str;
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap) {
    const char *fmt_c = format;
    char *out_c = str;
    char *end = (size == 0 ? 0 : str + size - 1);

    while (*fmt_c) {
        if (end && (out_c >= end)) {
            *end = 0;
            return end - str;
        }

        switch (*fmt_c) {
        case '%': {
            int precision = 0;
            uint flags = 0;
            ++fmt_c;

            bool changed;
            do {
                changed = true;
                switch (*fmt_c) {
                case '0':   flags |= ZERO_PADDED; ++fmt_c; break;
                case '+':   flags |= PRINT_PLUS; ++fmt_c; break;
                case ' ':   flags |= SPACE_PLUS; ++fmt_c; break;
                case '.':
                    fmt_c = sscan_int(++fmt_c, &precision, 10);
                    break;
                default:    
                    changed = false;
                }   
            } while (changed);

            switch (*fmt_c) {
            case 'x': {
                int arg = va_arg(ap, int);
                out_c = snprint_uint(out_c, end, arg, 16, flags, precision);
                } break;
            case 'd': case 'i': {
                int arg = va_arg(ap, int);
                out_c = snprint_int(out_c, end, arg, 10, flags, precision);
                } break;
            case 'u': {
                uint arg = va_arg(ap, uint);
                out_c = snprint_uint(out_c, end, arg, 10, flags, precision);
                } break;
            case 's': {
                char *c = va_arg(ap, char *);
                while (*c) {
                    if (end && (out_c >= end)) break;
                    *(out_c++) = *(c++);
                }
                } break;
            //case 'e': case 'f': case 
            default:
                *out_c = *fmt_c;
                ++out_c;
            }
            } break;
        default:
            *out_c = *fmt_c;
            ++out_c;
        }
        ++fmt_c;
    }
    *out_c = 0;
    return out_c - str;
}

int vsprintf(char *str, const char *format, va_list ap) {
    return vsnprintf(str, 0, format, ap);
}

int sprintf(char *str, const char *format, ...) {
    va_list vl;
    va_start(vl, format);
    int res = vsnprintf(str, 0, format, vl);
    va_end(vl);
    return res;
}



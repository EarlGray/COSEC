#ifndef __SYS_CONSOLE_H__
#define __SYS_CONSOLE_H__

#include <stdint.h>
#include <stdarg.h>

#define SCR_WIDTH	80
#define SCR_HEIGHT	25
#define TAB_INDENT	8

/*
 *
 */

#define N_VCSA_DEVICES  8

#define CONSOLE_VCSA    0
#define VIDEOMEM_VCSA   0x7f

enum vcsa_color {
    VCSA_ATTR_BLACK = 0,
    VCSA_ATTR_BLUE  = 1,
    VCSA_ATTR_GREEN = 2,
    VCSA_ATTR_CYAN  = 3,
    VCSA_ATTR_RED   = 4,
    VCSA_ATTR_MAGENTA = 5,
    VCSA_ATTR_ORANGE = 6,
    VCSA_ATTR_GREY  = 7,
};

#define BRIGHT(color)       (color + 0x8)
#define BACKGROUND(color)   (color * 0x10)

#define VCSA_DEFAULT_ATTRIBUTE  VCSA_ATTR_GREY

typedef struct devclass devclass;
devclass * get_vcs_family(void);

/*
 *  Direct buffer operation
 */
int vcs_current(void);

uint8_t vcsa_get_attribute(int vcsno);
void vcsa_set_attribute(int vcsno, uint8_t attr);

void vcsa_set_cursor(int vcsno, int x, int y);
void vcsa_get_cursor(int vcsno, int *x, int *y);

void vcsa_clear(int vcsno);
void vcsa_switch(int vcsno);

void vcsa_cprint(int vcsno, char c);

/* prints a preprocessed character to CONSOLE_VCSA */
void cprint(char c);

int k_vprintf(const char *fmt, va_list ap);
int k_printf(const char *fmt, ...);

/* legacy interface */
void print_centered(const char *s);

#endif // __SYS_CONSOLE_H__

#ifndef __SYS_CONSOLE_H__
#define __SYS_CONSOLE_H__

#include <stdint.h>

#define SCR_WIDTH	80
#define SCR_HEIGHT	25
#define TAB_INDENT	8

/* legacy interface */
void set_cursor_attr(uint8_t attr);
void set_default_cursor_attr(void);

void update_hw_cursor(void);
void clear_screen(void);

void cprint(char c);

void print_centered(const char *s);


/*
 *
 */

#define N_VCSA_DEVICES  8

#define VCSA_DEFAULT_ATTRIBUTES     0x07

enum vcs_ioctl_code {
    IOCTL_VCS_CLEAR = 0,
    IOCTL_VCS_CURSOR_SET,
    IOCTL_VCS_ATTR_SET,
};

typedef struct devclass devclass;
devclass * get_vcs_family(void);

/*
 *  Direct buffer operation
 */
void vcsa_set_attribute(int vcsno, uint8_t attr);
void vcsa_get_attribute(int vcsno, uint8_t *attr);

void vcsa_set_cursor(int vcsno, int x, int y);
void vcsa_get_cursor(int vcsno, int *x, int *y);

void vcsa_clear(int vcsno);
void vcsa_switch(int vcsno);

#endif // __SYS_CONSOLE_H__

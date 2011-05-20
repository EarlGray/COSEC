#ifndef __SYS_CONSOLE_H__
#define __SYS_CONSOLE_H__

#include <defs.h>

#define VIDEO_MEM	0x000B8000
#define SCR_WIDTH	80
#define SCR_HEIGHT	25
#define TAB_INDENT	8

#define videomem	((uint8_t *)VIDEO_MEM)

void update_hw_cursor(void);

void set_cursor_x(uint16_t X);
void set_cursor_y(uint16_t Y);
void set_cursor_attr(uint8_t attr);
void move_cursor_next();
void move_cursor_next_line();
uint16_t get_cursor_x();
uint16_t get_cursor_y();
uint8_t get_cursor_attr();

void putc_at(int x, int y, char c, char attr);
void print_at(int x, int y, const char *s);

void clear_screen(void);

void set_background(uint8_t bkgd);
void print_int(int x, uint8_t base);
void k_printf(const char *fmt, ...);
void scroll_up_line();

#endif // __SYS_CONSOLE_H__

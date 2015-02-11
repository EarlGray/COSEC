#ifndef __SYS_CONSOLE_H__
#define __SYS_CONSOLE_H__

#include <stdint.h>

#define VIDEO_MEM	0x000B8000
#define SCR_WIDTH	80
#define SCR_HEIGHT	25
#define TAB_INDENT	8

#define videomem	((uint8_t *)VIDEO_MEM)

void set_cursor_x(uint16_t X);
void set_cursor_y(uint16_t Y);
void set_cursor_attr(uint8_t attr);

uint16_t get_cursor_x();
uint16_t get_cursor_y();
uint8_t get_cursor_attr();

void update_hw_cursor(void);
void clear_screen(void);
void set_background(uint8_t bkgd);

void move_cursor_to(uint16_t cx, uint16_t cy);
void move_cursor_next();
void move_cursor_next_line();
void scroll_up_line();

void set_char_at(int x, int y, char c, char attr);
void cprint(char c);

void sprint_at(int x, int y, const char *s);

void print_uint(uint x, uint8_t base);
void print_int(int x, uint8_t base);

void print_centered(const char *s);

#endif // __SYS_CONSOLE_H__

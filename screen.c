#include <dev/screen.h>

#include <asm.h>


/***
  *     VGA 80x25 'driver'
 ***/

static uint16_t cursorX = 0;
static uint16_t cursorY = 0;
static uint8_t  cursorAttr = 0x07;

inline uint16_t get_cursor_x(void) { 	return cursorX;   }
inline uint16_t get_cursor_y(void) {  	return cursorY;   }
inline uint8_t get_cursor_attr() {  	return cursorAttr; }

inline void set_cursor_x(uint16_t X) { 	cursorX = X;  }
inline void set_cursor_y(uint16_t Y) { 	cursorY = Y;  }
inline void set_cursor_attr(uint8_t attr) {   cursorAttr = attr;   }


inline void update_hw_cursor(void)
{
    unsigned temp = cursorY * SCR_WIDTH + cursorX;

     /* This sends a command to indicies 14 and 15 in the
     *  CRT Control Register of the VGA controller. These
     *  are the high and low bytes of the index that show
     *  where the hardware cursor is to be 'blinking'. */
     outb(0x3D4, 14);
     outb(0x3D5, temp >> 8);
     outb(0x3D4, 15);
     outb(0x3D5, temp);
}


void set_background(uint8_t bkgd) {
	uint8_t *vc = (uint8_t *)videomem;
	int i, j;
	for (i = 0; i < SCR_HEIGHT; ++i)
		for (j = 0; j < SCR_WIDTH; ++j)
		{
			vc[1] = bkgd;
			vc = vc + 2;
		}	
}

void clear_screen(void) {
	uint8_t *pos = videomem;
	int i;
	for (i = 0; i < (SCR_WIDTH * SCR_HEIGHT); ++i)
	{
		pos[0] = 0x20;
		pos[1] = cursorAttr;
		pos += 2; 
	}
    move_cursor_to(0, 0);
}


inline void move_cursor_next() {
	++cursorX;	
	if (cursorX >= SCR_WIDTH) {
		cursorX = 0;
		move_cursor_next_line();
	}
    update_hw_cursor();
}

inline void move_cursor_to(uint16_t cx, uint16_t cy) {
    set_cursor_x(cx);
    set_cursor_y(cy);
    update_hw_cursor();
}

inline void move_cursor_next_line() {
	if (cursorY + 1 >= SCR_HEIGHT) 
		scroll_up_line();
	else ++cursorY;
}

void scroll_up_line() {
	uint8_t *s = (uint8_t *)VIDEO_MEM;
	int i, j;
	for (i = 0; i < SCR_HEIGHT-1; ++i)
		for (j = 0; j < SCR_WIDTH; ++j)
		{
			(*s) = *(s + SCR_WIDTH + SCR_WIDTH);
			++s;
			(*s) = *(s + SCR_WIDTH + SCR_WIDTH);
			++s;
		}
	for (i = 0; i < SCR_WIDTH; ++i) {
		*s = 0x20;
		++s;
		*s = cursorAttr;
		++s;
	}
}

inline void set_char_at(int x, int y, char c, char attr) {
	uint8_t* p = (uint8_t *)( VIDEO_MEM + ((x + y * SCR_WIDTH) << 1) );
	p[0] = c;
	p[1] = attr;
}

inline void cprint(char c) {
    switch (c) {
    case '\n': 
        set_cursor_x(0);
        move_cursor_next_line();
        break;
    case '\r':
        set_cursor_x(0);
        break;
    case '\t':
        set_cursor_x(TAB_INDENT * (1 + get_cursor_x()/TAB_INDENT));
        break;
    case '\b':
        if ((get_cursor_x() == 0) && (get_cursor_y() != 0))
            move_cursor_to(SCR_WIDTH - 1, get_cursor_y()-1);
        else
            set_cursor_x(get_cursor_x() - 1);
        break;
    default:
    	set_char_at(get_cursor_x(), get_cursor_y(), c, get_cursor_attr());
    	move_cursor_next();
    };
}

void sprint_at(int x, int y, const char *s) {
	const char *p = s;
	while (*p)
	{
		set_char_at(x, y, *p, cursorAttr);
		++p; 
		++x;
	}
}



inline char get_digit(uint8_t digit) {
	if (digit < 10) return('0' + digit);
	else return('A' + digit - 10);
}

void print_int(int x, uint8_t base) {
	if (x < 0) { 
        cprint('-');
        x = -x;
    }        
    print_uint(x, base);
}

void print_uint(uint x, uint8_t base) {
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
    while (n < maxDigits) {
        cprint(a[n]);
        n++;
    }
}

void k_printf(const char *fmt, ...) {
	const char *s = fmt;
	void *args = &fmt;
	args = (void *)(4 + (char *)args);
	while (*s) {
		switch (*s) {
		case '\n': 
			set_cursor_x(0);
			move_cursor_next_line();
			break;
		case '\r':
			set_cursor_x(0);
			break;
		case '\t':
			set_cursor_x(TAB_INDENT * (1 + get_cursor_x()/TAB_INDENT));
			break;
		case '\b':
            if ((get_cursor_x() == 0) && (get_cursor_y() != 0))
                move_cursor_to(SCR_WIDTH - 1, get_cursor_y()-1);
            else
			    set_cursor_x(get_cursor_x() - 1);
			break;
		case '%': 
			++s;
			switch (*s) {
			case 'd': 
	  			print_int(*(int *)args, 10);    
				args = (void *)((char *)args + 4);
				break;
            case 'u':
                print_uint(*(uint*)args, 10);
                args = (void *)((char *)args + 4);
			case 'x':
				print_uint(*(uint*)args, 16);
				args = (void *)((char *)args + 4);
				break;
			case 'o':
				print_uint(*(int*)args, 8);
				args = (void *)((char*)args + 4);
				break;
			case 's': {
                //k_printf(*(char **)args);
                char *c = *(char **)args;
                while (*c) 
                    cprint(*(c++));
				args = (void *)((char *)args + 4);
                } break;
            case '.':
                
                break;
			default:
				print_int(-42, 16);
			}
			break;
		default:
			cprint(*s);
		}
		++s;	
	}
    update_hw_cursor();
}



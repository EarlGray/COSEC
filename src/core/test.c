/*
 *      Caution: you examine this file at your peril,
 *  it might contain really bad temporary code.
 */

#include <ctype.h>
#include <misc/test.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <cosec/log.h>
#include <cosec/syscall.h>

#include "dev/timer.h"
#include "dev/kbd.h"
#include "dev/acpi.h"
#include "arch/i386.h"
#include "arch/intr.h"

#include "kshell.h"

void test_strs(void) {
    printf("strcmp('DUM', 'DUM') = %d\n", strcmp("DUM", "DUM"));
    printf("strcmp('DUM', 'DUM1') = %d\n", strcmp("DUM", "DUM1"));
    printf("strcmp('DUM', 'DU') = %d\n", strcmp("DUM", "DU"));
    printf("strcmp('DUM', 'DUN') = %d\n", strcmp("DUM", "DUN"));
    printf("'abc' 'ABC %d\n", strcasecmp("abc", "ABC"));
    printf("'ABC' 'abc %d\n", strcasecmp("ABC", "abc"));
    printf("'aa' 'AAA' %d\n", strcasecmp("aa", "AAA"));
    printf("'' 'a' %d\n", strcasecmp("", "a"));

    printf("atoi('0') = %d\n", atoi("0"));
    printf("atoi('1') = %d\n", atoi("1"));
    printf("atoi('-1') = %d\n", atoi("-1"));
    printf("atoi('42meaning') = %d\n", atoi("42meaning"));
    printf("atoi('123456') = %d\n", atoi("123456"));
    printf("atoi('what?') = %d\n", atoi("what?"));
    printf("atoi('') = %d\n", atoi(""));
}

static void test_vsprint(const char *fmt, ...) {
#define buf_size    200
    char buf[buf_size];
    va_list ap;
    va_start(ap, fmt);
//    vsnprintf(buf, buf_size, fmt, ap);

    const char *c = fmt;
    print_mem((const char *)&fmt, 0x100);
    while (*c) {
        switch (*c) {
        case '%':
            switch (*(++c)) {
                case 's': {
                    char *s = va_arg(ap, char *);
                    logmsgf("%%s: 0x%x, *s=0x%x\n", (uint)s, (uint)*s);
                          } break;
                case 'd': case 'u': case 'x': {
                    int arg = va_arg(ap, int);
                    logmsgf("%%d: 0x%x\n", (uint)arg);
                                              } break;
                default:
                    logmsg("Unknown operand for %%\n");
            }
        }
        ++c;
    }

    va_end(ap);
    logmsgf("\n%s\n", buf);
}

void test_sprintf(void) {
    test_vsprint("%s: %d, %s: %+x\n", "test1", -100, "test2", 200);
}

void test_usleep(void) {
    usleep(2 * 1000000);
    k_printf("Done\n\n");
}

void test_acpi(void) {
    acpi_init();
}


#if INTR_PROFILING
volatile union {
    uint32_t u32[2];
    uint64_t u64;
} intr_ticks = { .u64 = 0 };

volatile uint intr_count = 0;

extern uint intr_start_rdtsc(uint64_t *);

void intr_profile(uint64_t ts) {
    uint64_t start_ts;
    intr_start_rdtsc(&start_ts);
    int64_t diff = ts - start_ts;
    if (diff > 0)
        intr_ticks.u64 += diff;
    ++intr_count;
}
#endif


static void on_timer(uint counter) {
    if (counter % 100 == 0) {
        uint ts[2] = { 0 };
        i386_rdtsc((uint64_t *)ts);
        logmsgif("%d: rdtsc=%x %x ", counter, ts[1], ts[0]);
#if INTR_PROFILING
        logmsgif(" tick=%x %x, icnt = %x", intr_ticks.u32[1], intr_ticks.u32[0], intr_count);
#endif
        struct tm tm;
        time_ymd_from_rtc(&tm);
        logmsgif(" %d:%d:%d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    }
}

volatile bool poll_exit = false;

static void tt_keypress() {
    poll_exit = true;
}

void test_timer(void) {
    poll_exit = false;

    kbd_set_onpress((kbd_event_f)tt_keypress);
    timer_t timer_id = timer_push_ontimer(on_timer);

    while (!poll_exit) cpu_halt();

    timer_pop_ontimer(timer_id);
    kbd_set_onpress(null);
}


#include <dev/serial.h>
#include <dev/screen.h>
#include <dev/intrs.h>

static bool serialcode_mode = false;

inline static void print_serial_info(uint16_t port) {
    uint8_t iir;
    inb(port + 1, iir);
    logmsgf(" (IER=%x\t", (uint)iir);
    inb(port + 2, iir);
    logmsgf(" IIR=%x\t", (uint)iir);
    inb(port + 5, iir);
    logmsgf(" LSR=%x\t", (uint)iir);
    inb(port + 6, iir);
    logmsgf(" MSR=%x", (uint)iir);
    logmsgf(" PIC=%x) ", (uint)irq_get_mask());
}

void on_serial_received(uint8_t b) {
    vcsa_set_attribute(CONSOLE_VCSA, 0x0A);

    if (serialcode_mode) {
        k_printf(" %x", (uint)b);
    } else {
        switch (b) {
          case '\r': cprint('\n'); break;
          case 0x7f: cprint('\b'); break;
          default: cprint(b);
        }
    }
}

static void on_press(scancode_t scan) {
    switch (scan) {
        // SCAN_ESC
        case 0x01: poll_exit = true; return;
        // SCAN_DEL
        case 0x53: print_serial_info(COM1_PORT); return;
        // SCAN_F1
        case 0x3b: serialcode_mode = !serialcode_mode; return;
    }

    char c = translate_from_scan(null, scan);
    if (c == 0) return;

    while (! serial_is_transmit_empty(COM1_PORT));
    if (c == '\n')
        serial_write(COM1_PORT, '\r');

    while (! serial_is_transmit_empty(COM1_PORT));
    serial_write(COM1_PORT, c);

    vcsa_set_attribute(CONSOLE_VCSA, 0x0C);
    cprint(c);
}

void poll_serial() {
    poll_exit = false;
    kbd_set_onpress((kbd_event_f)on_press);

    while (!poll_exit) {
        if (serial_is_received(COM1_PORT)) {
            uint8_t b = serial_read(COM1_PORT);

            vcsa_set_attribute(CONSOLE_VCSA, 0x0A);
            cprint(b);
        }
    }
    kbd_set_onpress(null);
}

void test_serial(const char *arg) {
    UNUSED(arg);
    logmsgf("IRQs state = 0x%x\n", (uint)irq_get_mask());

    uint8_t saved_color = vcsa_get_attribute(VIDEOMEM_VCSA);

    k_printf("Use <Esc> to quit, <Del> for register info, <F1> to toggle char/code mode\n");
    serial_setup();

    //poll_serial();

    poll_exit = false;
    serial_set_on_receive(on_serial_received);
    kbd_set_onpress(on_press);

    while (!poll_exit) cpu_halt();

    kbd_set_onpress(null);
    serial_set_on_receive(null);

    vcsa_set_attribute(0, saved_color);
}

void test_kbd(void) {
    k_printf("Use <Esc> to quit\n");
    uint8_t key = 0;
    while (key != 1) {
        key = kbd_wait_scan(true);
        k_printf("%x->%x\t", (uint)key, (uint)translate_from_scan(null, key));
    }
    k_printf("\n");
}

/*
 *  Bochs/QEMU vga
 */
#define VBE_DISPI_IOPORT_INDEX  0x01CE
#define VBE_DISPI_IOPORT_DATA   0x01CF

enum vbe_index {
    VBE_DISPI_INDEX_ID = 0,
    VBE_DISPI_INDEX_XRES = 1,
    VBE_DISPI_INDEX_YRES = 2,
    VBE_DISPI_INDEX_BPP = 3,
    VBE_DISPI_INDEX_ENABLE = 4,
    VBE_DISPI_INDEX_BANK = 5,
    VBE_DISPI_INDEX_VIRT_WIDTH = 6,
    VBE_DISPI_INDEX_VIRT_HEIGHT = 7,
    VBE_DISPI_INDEX_X_OFFSET = 8,
    VBE_DISPI_INDEX_Y_OFFSET = 9,
};

static inline uint16_t vbe_get_index(enum vbe_index index) {
    outw_p(VBE_DISPI_IOPORT_INDEX, (uint16_t)index);
    uint16_t val;
    inw_p(VBE_DISPI_IOPORT_DATA, val);
    return val;
}

static inline void vbe_set_index(enum vbe_index index, uint16_t value) {
    outw_p(VBE_DISPI_IOPORT_INDEX, (uint16_t)index);
    outw_p(VBE_DISPI_IOPORT_DATA, value);
}

void test_vga(const char *arg) {
    uint16_t id = vbe_get_index(VBE_DISPI_INDEX_ID);
    logmsgif("%s: ID = 0x%x\n", __func__, id);
    if (id < 0xB0C0) {
        logmsgef("%s: BGA does not work", __func__);
        return;
    }
    vbe_set_index(VBE_DISPI_INDEX_ENABLE, 3); // enable + capabilities
    uint16_t maxw = vbe_get_index(VBE_DISPI_INDEX_XRES);
    uint16_t maxh = vbe_get_index(VBE_DISPI_INDEX_YRES);
    logmsgif("%s: max resolution is %dx%d", __func__, maxw, maxh);

    vbe_set_index(VBE_DISPI_INDEX_ENABLE, 0);

    uint32_t w = 640;
    uint32_t h = 480;
    uint32_t d = 32;
    arg = sscan_uint(arg, &w, 10);  while (isspace(*arg)) ++arg;
    arg = sscan_uint(arg, &h, 10);  while (isspace(*arg)) ++arg;
    arg = sscan_uint(arg, &d, 10);
    logmsgf("%s: width=%d, height=%d, bpp=%d\n", __func__, w, h, d);

    vbe_set_index(VBE_DISPI_INDEX_XRES, w);
    vbe_set_index(VBE_DISPI_INDEX_YRES, h);
    vbe_set_index(VBE_DISPI_INDEX_BPP, d);
    /* TODO: verify that the resolution is set */

    vbe_set_index(VBE_DISPI_INDEX_ENABLE, 1);

    vbe_set_index(VBE_DISPI_INDEX_BANK, 2);

    uint32_t *videomem = (uint32_t *)0xa0000;
    for (uint32_t i = 0; i < w/2; ++i) {
        videomem[i+i] = 0x00ffff;
    }
}


/***
  *     Example tasks
 ***/
#include "tasks.h"

uint8_t task0_stack[TASK_KERNSTACK_SIZE];
uint8_t task1_stack[TASK_KERNSTACK_SIZE];

#define R0_STACK_SIZE       0x400
#define R3_STACK_SIZE       0x1000
uint8_t task0_usr_stack[R3_STACK_SIZE];
uint8_t task1_usr_stack[R3_STACK_SIZE];

volatile task_struct task0;
volatile task_struct task1;
task_struct *volatile def_task;


void do_task0(void) {
    k_printf("do_task0: CPL = %d\n", (int)i386_current_privlevel());
    int i = 0;
    while (1) {
        ++i;
        if (0 == (i % 75)) {    i = 0;   k_printf("\r");    }
        if (i > 75) {
            logmsgf("\nA: assert i <= 75 failed, i=0x%x\n", i);
            while (1) cpu_halt();
        }
        k_printf("0");
    }
}

void do_task1(void) {
    int i = 0;
    while (1) {
        ++i;
       if (0 == (i % 75)) {   i = 0;   k_printf("\r");   }
       if (i > 75) {
           logmsgf("\nB: assert i <= 75 failed, i=0x%x\n", i);
           while (1) cpu_halt();
       }
       k_printf("1");
    }
}

volatile bool quit;

void key_press(/*scancode_t scan*/) {
    logmsgif("\nkey pressed...\n");
    quit = true;
}


task_struct *next_task(/*uint tick*/) {
    if (quit) return (task_struct *)def_task;

    task_struct *current = task_current();
    if (current == def_task) return (task_struct *)&task0;
    //if (tick % 1000) return NULL;

    if (current == &task0) return (task_struct *)&task1;
    if (current == &task1) return (task_struct *)&task0;
    panic("Invalid task");
    return null;
}

void test_tasks(void) {
    def_task = task_current();

#if 1
    task_kthread_init((task_struct *)&task0, (void *)do_task0,
            (task0_stack + TASK_KERNSTACK_SIZE - 0x20));
    task_kthread_init((task_struct *)&task1, (void *)do_task1,
            (task1_stack + TASK_KERNSTACK_SIZE - 0x20));
#else
	/* this does not work because userspace tasks
	 * cannot access their code in kernel space
	 */
    const segment_selector ucs = { .as.word = SEL_USER_CS };
    const segment_selector uds = { .as.word = SEL_USER_DS };

    uint espU0 = ((uint)task0_usr_stack + R3_STACK_SIZE - 0x18);
    uint espK0 = ((uint)task0_stack + TASK_KERNSTACK_SIZE - CONTEXT_SIZE - 0x14);

    uint espU1 = ((uintptr_t)task1_usr_stack + R3_STACK_SIZE - 0x18);
    uint espK1 = ((uintptr_t)task1_stack + TASK_KERNSTACK_SIZE - CONTEXT_SIZE - 0x14);

    task_init((task_struct *)&task0, (void *)do_task0,
            (void *)espK0, (void *)espU0, ucs, uds);
    task_init((task_struct *)&task1, (void *)do_task1,
            (void *)espK1, (void *)espU1, ucs, uds);
#endif

    /* allow tasks to update cursor with `outl` */
    task0.tss.eflags |= eflags_iopl(PL_USER);
    task1.tss.eflags |= eflags_iopl(PL_USER);

    quit = false;
    kbd_set_onpress((kbd_event_f)key_press);
    task_set_scheduler(next_task);

    /* wait for first timer tick, when execution will be transferred to do_task0 */
    cpu_halt();

    task_set_scheduler(null);
    kbd_set_onpress(null);

    k_printf("\nBye.\n");
}

/***********************************************************/
uint32_t test_syscall(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3);

static uint32_t test_sys_write(int fd, const char *buf, size_t len) {
    return test_syscall(SYS_WRITE, (uint32_t)fd, (uint32_t)buf, (uint32_t)len);
}

static uint32_t test_sys_read(int fd, char *buf, size_t buflen) {
    return test_syscall(SYS_READ, (uint32_t)fd, (uint32_t)buf, (uint32_t)buflen);
}

void run_userspace(void) {
    size_t nbuf;
    char buf[100];

    if (i386_current_privlevel() != 3) {
        logmsgef("failed to enter userspace");
        return;
    }

    nbuf = snprintf(buf, 100, "\x1b" "c\n\t\t\tWelcome to userspace!\n\n", i386_current_privlevel());
    test_sys_write(STDOUT_FILENO, buf, nbuf);

    for (;;) {
        snprintf(buf, 100, "echo> ");
        nbuf = strlen(buf);
        test_sys_write(STDOUT_FILENO, buf, strlen(buf));

        nbuf = test_sys_read(STDIN_FILENO, buf, 100);
        buf[nbuf] = '\0';
        test_sys_write(STDOUT_FILENO, buf, strlen(buf));
    }
}

task_struct task3;

void test_userspace(void) {
    /* init task */
    task3.tss.eflags = i386_eflags(); // | eflags_iopl(PL_USER);
    task3.tss.cs = SEL_USER_CS;
    task3.tss.ds = task3.tss.es = task3.tss.fs = task3.tss.gs = SEL_USER_DS;
    task3.tss.ss = SEL_USER_DS;
    task3.tss.esp = (uint)task0_usr_stack + R3_STACK_SIZE - CONTEXT_SIZE - 0x20;
    task3.tss.eip = (uint)run_userspace;
    task3.tss.ss0 = SEL_KERN_DS;
    task3.tss.esp0 = (uint)task0_stack + R0_STACK_SIZE - CONTEXT_SIZE - 0x20;

    /* make a GDT task descriptor */
    segment_descriptor taskdescr;
    segdescr_taskstate_init(taskdescr, (uint)&(task3.tss), PL_USER);
    segdescr_taskstate_busy(taskdescr, 0);

    index_t taskdescr_index = gdt_alloc_entry(taskdescr);

    segment_selector tss_sel;
    tss_sel.as.word = make_selector(taskdescr_index, 0, taskdescr.as.strct.dpl);

    uint efl = 0x00203202;
    logmsgf("flags=0x%x\n", i386_eflags());
    logmsgf("efl = 0x%x\n", efl);
    logmsgf("tss_sel = 0x%x\n", (uint)tss_sel.as.word);
    logmsgf("tssd = %x %x\n", taskdescr.as.ints[0], taskdescr.as.ints[1]);
    logmsgf("tssd.base = %x\n", taskdescr.as.strct.base_l +
            (taskdescr.as.strct.base_m << 16) + (taskdescr.as.strct.base_h << 24));

    /* load TR and LDT */
    i386_load_task_reg(tss_sel);
    //asm ("lldt %%ax     \n\t"::"a"( SEL_DEF_LDT ));

    /* go userspace */
    i386_iret(
        (uint)run_userspace, task3.tss.cs,
        efl,
        task3.tss.esp, task3.tss.ss
    );
}

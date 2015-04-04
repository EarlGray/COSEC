/*
 *      Caution: you examine this file at your peril,
 *  it might contain really bad temporary code.
 */

#include <misc/test.h>

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <dev/timer.h>
#include <dev/kbd.h>
#include <arch/i386.h>
#include <fs/fs_sys.h>

#include <kshell.h>
#include <log.h>

void test_strs(void) {
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

void test_eflags(void) {
    uint flags = 0;
    i386_eflags(flags);
    logmsgf("flags=0x%x\n", flags);
}

void test_usleep(void) {
    usleep(2 * 1000000);
    k_printf("Done\n\n");
}


#if INTR_PROFILING
volatile union {
    uint32_t u32[2];
    uint64_t u64;
} intr_ticks = { .u32 = { 0 } };

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


/***
  *     Example tasks
 ***/
#include <tasks.h>

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
    if (current == def_task) return &task0;
    //if (tick % 1000) return NULL;

    if (current == &task0) return &task1;
    if (current == &task1) return &task0;
    panic("Invalid task");
    return null;
}

void test_tasks(void) {
    def_task = task_current();

#if 0
    task_kthread_init(&task0, (void *)do_task0,
            (void *)((ptr_t)task0_stack + TASK_KERNSTACK_SIZE - 0x20));
    task_kthread_init(&task1, (void *)do_task1,
            (void *)((ptr_t)task1_stack + TASK_KERNSTACK_SIZE - 0x20));
#else
    const segment_selector ucs = { .as.word = SEL_USER_CS };
    const segment_selector uds = { .as.word = SEL_USER_DS };

    uint espU0 = ((uint)task0_usr_stack + R3_STACK_SIZE - 0x18);
    uint espK0 = ((uint)task0_stack + TASK_KERNSTACK_SIZE - CONTEXT_SIZE - 0x14);

    uint espU1 = ((ptr_t)task1_usr_stack + R3_STACK_SIZE);
    uint espK1 = ((ptr_t)task1_stack + TASK_KERNSTACK_SIZE - CONTEXT_SIZE - 0x14);

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
void run_userspace(void) {
    char buf[100];
    snprintf(buf, 100, "Current privilege level = %d\n", i386_current_privlevel());
    size_t nbuf = strlen(buf);

    asm("int $0x80 \n" ::
            "a"(SYS_WRITE),
            "c"(STDOUT_FILENO),
            "d"((uint)buf),
            "b"(nbuf));
    while (1);
}

extern void start_userspace(uint eip3, uint cs3, uint eflags, uint esp3, uint ss3);

task_struct task3;

void test_userspace(void) {
    /* init task */
    task3.tss.eflags = x86_eflags(); // | eflags_iopl(PL_USER);
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

    kbd_set_onpress((kbd_event_f)key_press);

    uint efl = 0x00203202;
    test_eflags();
    logmsgf("efl = 0x%x\n", efl);
    logmsgf("tss_sel = 0x%x\n", (uint)tss_sel.as.word);
    logmsgf("tssd = %x %x\n", taskdescr.as.ints[0], taskdescr.as.ints[1]);
    logmsgf("tssd.base = %x\n", taskdescr.as.strct.base_l +
            (taskdescr.as.strct.base_m << 16) + (taskdescr.as.strct.base_h << 24));

    /* load TR and LDT */
    i386_load_task_reg(tss_sel);
    //asm ("lldt %%ax     \n\t"::"a"( SEL_DEF_LDT ));

    /* go userspace */
    start_userspace(
        (uint)run_userspace, task3.tss.cs,
        //(uint)run_userspace, (uint)tss_sel.as.word,
        efl,
        task3.tss.esp, task3.tss.ss);
}


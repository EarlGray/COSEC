/*
 *      Caution: you examine this file at your peril,
 *  it might contain really bad temporary code.
 */

#include <misc/test.h>
#include <std/string.h>
#include <std/stdarg.h>
#include <std/stdio.h>
#include <dev/timer.h>
#include <arch/i386.h>
#include <kshell.h>

volatile bool poll_exit = false;

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

void test_eflags(void) {
    uint flags = 0;
    i386_eflags(flags);
    k_printf("flags=0x%x\n", flags);
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
    if (counter % 1000 == 0) {
        uint ts[2] = { 0 };
        i386_rdtsc(ts);
        k_printf("%d: rdtsc=%x %x ", counter, ts[1], ts[0]);
#if INTR_PROFILING
        k_printf(" itick=%x %x, icnt = %x", intr_ticks.u32[1], intr_ticks.u32[0], intr_count);
#endif
        k_printf("\n");
    }
}

static void tt_keypress() {
    poll_exit = true;
}

void test_timer(void) {
    poll_exit = false;

    kbd_set_onpress(tt_keypress);
    timer_t timer_id = timer_push_ontimer(on_timer);

    while (!poll_exit) cpu_halt();

    timer_pop_ontimer(timer_id);
    kbd_set_onpress(null);
}


#include <dev/serial.h>
#include <dev/kbd.h>
#include <dev/screen.h>
#include <dev/intrs.h>


inline static void print_serial_info(uint16_t port) {
    uint8_t iir;
    inb(port + 1, iir);
    k_printf("(IER=%x\t", (uint)iir);
    inb(port + 2, iir);
    k_printf("IIR=%x\t", (uint)iir);
    inb(port + 5, iir);
    k_printf("LSR=%x\t", (uint)iir);
    inb(port + 6, iir);
    k_printf("MSR=%x", (uint)iir);
    k_printf("PIC=%x)", (uint)irq_get_mask());
}

void on_serial_received(uint8_t b) {
    set_cursor_attr(0x0A);
    cprint(b);
    update_hw_cursor();
}

static void on_press(scancode_t scan) {
    if (scan == 1) 
        poll_exit = true;
    if (scan == 0x53) 
        print_serial_info(COM1_PORT);

    char c = translate_from_scan(null, scan);
    if (c == 0) return;

    while (! serial_is_transmit_empty(COM1_PORT));
    serial_write(COM1_PORT, c);
    
    set_cursor_attr(0x0C);
    cprint(c);
    update_hw_cursor();
}

void poll_serial() {
    poll_exit = false;
    kbd_set_onpress(on_press);

    while (!poll_exit) {
        if (serial_is_received(COM1_PORT)) {
            uint8_t b = serial_read(COM1_PORT);

            set_cursor_attr(0x0A);
            cprint(b);
            update_hw_cursor();
        }
    }
    kbd_set_onpress(null);
}

void test_serial(void) {
    k_printf("IRQs state = 0x%x\n", (uint)irq_get_mask());

    uint8_t saved_color = get_cursor_attr();
    k_printf("Use <Esc> to quit, <Del> for register info\n");
    serial_setup();

    //poll_serial();
    
    poll_exit = false;
    serial_set_on_receive(on_serial_received);
    kbd_set_onpress(on_press);

    while (!poll_exit) cpu_halt();

    kbd_set_onpress(null);
    serial_set_on_receive(null);
    set_cursor_attr(saved_color);
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

task_struct task0;
task_struct task1; 
task_struct *volatile def_task;


void do_task0(void) {
    int i = 0;
    while (1) {
        ++i;
        if (0 == (i % 75)) {    i = 0;   k_printf("\r");    }
        if (i > 75) {
            k_printf("\nA: assert i <= 75 failed, i=0x%x\n", i);
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
           k_printf("\nB: assert i <= 75 failed, i=0x%x\n", i);
           while (1) cpu_halt();
       }
       k_printf("1");
    }
}

volatile bool quit;

void key_press(/*scancode_t scan*/) {
    k_printf("\n\nexiting...\n");
    quit = true;
}


bool switch_context() {
    return true; 
}

task_struct *next_task(void) {
    task_struct *current = task_current();
    if (quit) return def_task;
    if (current == &task0) return &task1;
    if (current == &task1) return &task0;
    if (current == def_task) return &task0;
    panic("Invalid task");
    return null;
}


void test_tasks(void) {
    def_task = task_current();

    task_init(&task0, (void *)do_task0, 
            (void *)((ptr_t)task0_stack + TASK_KERNSTACK_SIZE - 0x20));
    task_init(&task1, (void *)do_task1, 
            (void *)((ptr_t)task1_stack + TASK_KERNSTACK_SIZE - 0x20));

    quit = false;
    kbd_set_onpress((kbd_event_f)key_press); 
    task_set_scheduler(switch_context, next_task);

    /* wait for first timer tick, when execution will be transferred to do_task0 */
    cpu_halt(); 
    
    kbd_set_onpress(null);
    task_set_scheduler(null, null);

    k_printf("\nBye.\n");
}

/***********************************************************/

#define R3_STACK_SIZE       0x1000
#define R0_STACK_SIZE       0x400
uint8_t task_stack3[R3_STACK_SIZE];
uint8_t task_stack0[R0_STACK_SIZE];

void run_userspace(void) {
    k_printf("Hello, userspace");
    while (1);
}

extern void start_userspace(uint eip3, uint cs3, uint eflags, uint esp3, uint ss3);

task_struct task3;

void test_userspace(void) {
    /* init task */
    task3.tss.eflags = x86_eflags() | eflags_iopl(PL_USER);
    task3.tss.cs = SEL_USER_CS;
    task3.tss.ds = task3.tss.es = task3.tss.fs = task3.tss.gs = SEL_USER_DS;
    task3.tss.ss = SEL_USER_DS;
    task3.tss.eip = (uint)run_userspace;
    task3.tss.ss0 = SEL_KERN_DS;
    task3.tss.esp0 = (uint)task_stack0 + R0_STACK_SIZE - 0x20;

    /* make a GDT task descriptor */
    segment_descriptor taskdescr;
    segdescr_taskstate_init(taskdescr, (uint)&task3, PL_USER);

    index_t taskdescr_index = gdt_alloc_entry(taskdescr);

    segment_selector tss_sel;
    tss_sel.as.word = make_selector(taskdescr_index, 0, PL_USER);

    k_printf("GDT[%x]\n", taskdescr_index);

    /* load TR and LDT */
    asm ("ltrw %%ax     \n\t"::"a"( tss_sel ));
    asm ("lldt %%ax     \n\t"::"a"( SEL_DEFAULT_LDT ));
    
    /* go userspace */
    start_userspace(
        (uint)run_userspace, SEL_USER_CS,
        x86_eflags() | eflags_iopl(PL_USER),
        (uint)task_stack3 + R3_STACK_SIZE - 0x20, SEL_USER_DS);
}

#include <dev/timer.h>

#include <dev/intrs.h>
#include <arch/i386.h>

#include <stdlib.h>
#include <string.h>

#define PIT_CH0_PORT    0x40
#define PIT_CH1_PORT    0x41
#define PIT_CH3_PORT    0x42
#define PIT_CMD_PORT    0x43

volatile uint timer_freq_divisor = 0x100;
volatile ulong ticks = 0;

timer_event_f timers[N_TIMERS] = { 0 };

timer_t timer_push_ontimer(timer_event_f ontimer) {
    int i = 0;
    for (i = 0; i < N_TIMERS; ++i)
        if (null == timers[i]) {
            timers[i] = ontimer;
            return i;
        }
    return i;
}

ulong timer_ticks(void) {
    return ticks;
}

void timer_pop_ontimer(timer_t id) {
    timers[id] = null;
}

void timer_set_frequency(uint hz) {
    uint divisor = PIT_MAX_FREQ / hz;
    timer_freq_divisor = divisor;
    outb(PIT_CMD_PORT, 0x36);
    outb(PIT_CH0_PORT, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH0_PORT, (uint8_t)(divisor >> 8));
}

uint timer_frequency(void) {
    return PIT_MAX_FREQ / timer_freq_divisor;
}

void timer_setup(void) {
    timer_set_frequency(timer_freq_divisor);

    irq_set_handler(TIMER_IRQ, timer_irq);
    irq_mask(TIMER_IRQ, true);
}

void timer_irq() {
    ++ ticks;

    int i;
    for(i = 0; i < N_TIMERS; ++i)
        if (timers[i])
            timers[i](ticks);
}

int usleep(useconds_t usec) {
    ulong tick0 = timer_ticks();
    uint dt = (1000000.0 * timer_freq_divisor) / (float)PIT_MAX_FREQ;
    while (1) {
        cpu_halt(); // yield()
        ulong tick = timer_ticks();
        if (dt * (tick - tick0) > usec) 
           return 0; 
    }
}

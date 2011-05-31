#include <dev/timer.h>

#include <dev/intrs.h>

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

void timer_pop_ontimer(timer_t id) {
    timers[id] = null;
}

void timer_setup(void) {
    memset(timers, 0, N_TIMERS * sizeof(timer_event_f));

    irq_set_handler(0, timer_irq);
}

void timer_irq() {
    static uint32_t counter = 0;
    ++counter;

    /*int i;
    for (i = 0; i < N_TIMERS; ++i)
        timers[i](counter); */
}


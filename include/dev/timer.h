#ifndef __TIMER_H__
#define __TIMER_H__

#define N_TIMERS        20
#define PIT_MAX_FREQ    1193180

typedef void (*timer_event_f)(uint);
typedef uint timer_t;

/*** 
  *  adds timer event handler
  *  returns timer ID, which has to be stored to delete it 
  *    if N_TIMERS, event can't be enabled
 **/
timer_t timer_push_ontimer(timer_event_f ontimer);

/***
  * deletes a timer event with id
 ***/
void timer_pop_ontimer(timer_t id);

void timer_set_frequency(uint hz);
uint timer_frequency(void);

void timer_setup(void);
void timer_irq();

#endif //__TIMER_H__

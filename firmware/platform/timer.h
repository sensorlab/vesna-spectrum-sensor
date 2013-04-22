#ifndef HAVE_TIMER_H
#define HAVE_TIMER_H

int vss_timer_init(void);
int vss_timer_schedule(uint32_t delay_ms);
void vss_timer_ack(void);
#endif

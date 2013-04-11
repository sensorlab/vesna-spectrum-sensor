#ifndef HAVE_RTC_H
#define HAVE_RTC_H

#include <stdint.h>

int vss_rtc_init(void);
int vss_rtc_reset(void);
uint32_t vss_rtc_read(void);

#endif

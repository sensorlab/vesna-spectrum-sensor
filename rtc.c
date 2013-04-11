#include <libopencm3/stm32/f1/rtc.h>

#include "vss.h"
#include "rtc.h"

int vss_rtc_init(void)
{
	rtc_awake_from_off(LSE);
	rtc_set_prescale_val(15);

	return VSS_OK;
}

int vss_rtc_reset(void)
{
	rtc_set_counter_val(0);

	return VSS_OK;
}

uint32_t vss_rtc_read(void)
{
	return rtc_get_counter_val();
}

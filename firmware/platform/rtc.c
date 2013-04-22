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
	uint32_t val = rtc_get_counter_val();
	/* LSE clock is 32768 Hz. Prescaler is set to 16.
	 *
	 *                 rtc_counter * 16
	 * t [ms] = 1000 * ----------------
	 *                       32768
	 */
	return ((long long) val) * 1000 / 2048;
}

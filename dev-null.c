#include <stdlib.h>
#include <libopencm3/stm32/f1/rtc.h>

#include "spectrum.h"

int dev_null_reset(void* priv) 
{
	return E_SPECTRUM_OK;
}

int dev_null_setup(void* priv, const struct spectrum_sweep_config* sweep_config) 
{
	return E_SPECTRUM_OK;
}

int dev_null_run(void* priv, const struct spectrum_sweep_config* sweep_config)
{
	int r;
	short int *data;

	rtc_set_counter_val(0);

	data = calloc(spectrum_sweep_channel_num(sweep_config), sizeof(*data));
	if (data == NULL) {
		return E_SPECTRUM_TOOMANY;
	}

	do {
		uint32_t rtc_counter = rtc_get_counter_val();
		/* LSE clock is 32768 Hz. Prescaler is set to 16.
		 *
		 *                 rtc_counter * 16
		 * t [ms] = 1000 * ----------------
		 *                       32768
		 */
		int timestamp = ((long long) rtc_counter) * 1000 / 2048;

		r = sweep_config->cb(sweep_config, timestamp, data);
	} while(!r);

	free(data);

	if (r == E_SPECTRUM_STOP_SWEEP) {
		return E_SPECTRUM_OK;
	} else {
		return r;
	}
}

const struct spectrum_dev_config dev_null_config_1 = {
	.name			= "config 1",

	.channel_base_hz 	= 20,
	.channel_spacing_hz	= 1,
	.channel_bw_hz		= 1,
	.channel_num		= 20000,

	.channel_time_ms	= 100,

	.priv			= NULL,
};

const struct spectrum_dev_config dev_null_config_2 = {
	.name			= "config 2",

	.channel_base_hz 	= 20,
	.channel_spacing_hz	= 1,
	.channel_bw_hz		= 10,
	.channel_num		= 20000,

	.channel_time_ms	= 100,

	.priv			= NULL
};

const struct spectrum_dev_config* dev_null_config_list[] = { &dev_null_config_1, &dev_null_config_2 };

const struct spectrum_dev dev_null = {
	.name = "null device",

	.dev_config_list	= dev_null_config_list,
	.dev_config_num		= 2,

	.dev_reset		= dev_null_reset,
	.dev_setup		= dev_null_setup,
	.dev_run		= dev_null_run,

	.priv 			= NULL
};

int dev_null_register(void) {
	return spectrum_add_dev(&dev_null);
}

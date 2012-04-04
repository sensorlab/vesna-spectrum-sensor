/* Copyright (C) 2012 SensorLab, Jozef Stefan Institute
 * http://sensorlab.ijs.si
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>. */

/* Author: Tomaz Solc, <tomaz.solc@ijs.si> */
#include <stdlib.h>
#include <libopencm3/stm32/f1/rtc.h>

#include "spectrum.h"

int dev_dummy_reset(void* priv)
{
	return E_SPECTRUM_OK;
}

int dev_dummy_setup(void* priv, const struct spectrum_sweep_config* sweep_config) 
{
	return E_SPECTRUM_OK;
}

typedef short int (*data_f)(void);

static short int get_zero(void)
{
	return 0;
}

static short int get_random(void)
{
	return -(rand() % 10000);
}

int dev_dummy_run(void* priv, const struct spectrum_sweep_config* sweep_config)
{
	int r, channel_num, n;
	short int *data;

	rtc_set_counter_val(0);

	channel_num = spectrum_sweep_channel_num(sweep_config); 
	data = calloc(channel_num, sizeof(*data));
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

		for(n = 0; n < channel_num; n++) {
			data[n] = ((data_f) sweep_config->dev_config->priv)();
		}

		r = sweep_config->cb(sweep_config, timestamp, data);
	} while(!r);

	free(data);

	if (r == E_SPECTRUM_STOP_SWEEP) {
		return E_SPECTRUM_OK;
	} else {
		return r;
	}
}

const struct spectrum_dev_config dev_dummy_null_config = {
	.name			= "returns 0 dBm",

	.channel_base_hz 	= 0,
	.channel_spacing_hz	= 1,
	.channel_bw_hz		= 1,
	.channel_num		= 1000000000,

	.channel_time_ms	= 0,

	.priv			= get_zero
};

const struct spectrum_dev_config dev_dummy_random_config = {
	.name			= "return random power between 0 and -100 dBm",

	.channel_base_hz 	= 0,
	.channel_spacing_hz	= 1,
	.channel_bw_hz		= 1,
	.channel_num		= 1000000000,

	.channel_time_ms	= 0,

	.priv			= get_random
};

const struct spectrum_dev_config* dev_dummy_config_list[] = {
	&dev_dummy_null_config, 
	&dev_dummy_random_config };

const struct spectrum_dev dev_dummy = {
	.name = "dummy device",

	.dev_config_list	= dev_dummy_config_list,
	.dev_config_num		= 2,

	.dev_reset		= dev_dummy_reset,
	.dev_setup		= dev_dummy_setup,
	.dev_run		= dev_dummy_run,

	.priv 			= NULL
};

int dev_dummy_register(void) {
	return spectrum_add_dev(&dev_dummy);
}

/* Copyright (C) 2013 SensorLab, Jozef Stefan Institute
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
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <limits.h>
#include <tda18219/tda18219.h>
#include <tda18219/tda18219regs.h>

#include "ad8307.h"
#include "average.h"
#include "calibration.h"
#include "device.h"
#include "eeprom.h"
#include "ltc1560.h"
#include "rtc.h"
#include "task.h"
#include "tda18219.h"
#include "vss.h"

#include "device-tda18219.h"

enum state_t {
	OFF,
	RUN_MEASUREMENT,
	READ_MEASUREMENT,
	SET_FREQUENCY };

static enum state_t current_state = OFF;
static struct vss_task* current_task = NULL;

struct dev_tda18219_priv {
	const struct tda18219_standard* standard;
	const struct calibration_point* calibration;
	int adc_source;
	int bwsel;
};

static int get_input_power_bband(int* rssi_dbm_100, unsigned int n_average)
{
	uint16_t samples[n_average];
	int r = vss_ad8307_get_input_samples(samples, n_average);
	if(r) return r;

	*rssi_dbm_100 = vss_signal_power(samples, n_average);

	return VSS_OK;
}

static int get_input_power_det(int* rssi_dbm_100, unsigned int n_average)
{
	uint8_t rssi_dbuv;

	int r = tda18219_get_input_power_read(&rssi_dbuv);
	if(r) return r;

	if(rssi_dbuv < 40) {
		// internal power detector in TDA18219 doesn't go below 40 dBuV

		uint16_t samples[n_average];
		r = vss_ad8307_get_input_samples(samples, n_average);
		if(r) return r;

		/* STM32F1 has a 12 bit AD converter. Low reference is 0 V, high is 3.3 V
		 *
		 *              3.3 V
		 *     Kad = ----------
		 *           (2^12 - 1)
		 *
		 * AD8307
		 *
		 *     Kdet = 25 mV/dB  (slope)
		 *     Adet = -84 dBm   (intercept)
		 *
		 * Since we are using this detector for low power signals only, TDA18219 is
		 * at maximum gain.
		 *
		 *     AGC1 = 15 dB
		 *     AGC2 = -2 dB
		 *     AGC3 = 30 dB (guess based on measurement)
		 *     AGC4 = 14 dB
		 *     AGC5 = 9 dB
		 *     --------------
		 *     Atuner = 66 dB
		 *
		 * Pinput [dBm] = N * Kad / Kdet - Adet - Atuner
		 *
		 *                  3.3 V * 1000
		 *              = N ------------ - 84 - 66
		 *                  4095 * 25 V
		 *
		 * Note we are returning [dBm * 100]
		 */
		power_t buffer[n_average];
		unsigned int n;
		for(n = 0; n < n_average; n++) {
			buffer[n] = samples[n] * 3300 / 1024 - 15000;
		}

		*rssi_dbm_100 = vss_average(buffer, n_average);
	} else {
		// P [dBm] = U [dBuV] - 90 - 10 log 75 ohm
		*rssi_dbm_100 = ((int) rssi_dbuv) * 100 - 9000 - 1875;
	}

	return VSS_OK;
}

static int get_input_power(int* rssi_dbm_100, unsigned int n_average,
		int adc_source)
{
	switch(adc_source) {
		case AD8307_SRC_DET:
			return get_input_power_det(rssi_dbm_100, n_average);
		case AD8307_SRC_BBAND:
			return get_input_power_bband(rssi_dbm_100, n_average);
		default:
			assert(0);
	}
}

static int vss_device_tda18219_init(void)
{
	int r;

	r = vss_rtc_init();
	if(r) return r;

	r = vss_tda18219_init();
	if(r) return r;

	r = vss_ad8307_init();
	if(r) return r;

	r = vss_ltc1560_init();
	if(r) return r;

	r = tda18219_power_on();
	if(r) return VSS_ERROR;

	r = tda18219_init();
	if(r) return VSS_ERROR;

	r = tda18219_power_standby();
	if(r) return VSS_ERROR;

	return VSS_OK;
}

int dev_tda18219_turn_on(const struct dev_tda18219_priv* priv)
{
	int r;
	r = tda18219_power_on();
	if(r) return VSS_ERROR;

	r = tda18219_set_standard(priv->standard);
	if(r) return VSS_ERROR;

	r = vss_ad8307_power_on(priv->adc_source);
	if(r) return r;

	r = vss_ltc1560_bwsel(priv->bwsel);
	if(r) return r;

	return VSS_OK;
}

int dev_tda18219_turn_off(void)
{
	int r;
	r = vss_ad8307_power_off();
	if(r) return r;

	r = tda18219_power_standby();
	if(r) return VSS_ERROR;

	return VSS_OK;
}

static int dev_tda18219_start(struct vss_task* task) 
{
	int r = dev_tda18219_turn_on(task->sweep_config->device_config->priv);
	if(r) return r;

	current_task = task;

	return VSS_OK;
}

static int dev_tda18219_stop(void)
{
	current_task = NULL;

	return dev_tda18219_turn_off();
}

enum state_t dev_tda18219_state(struct vss_task* task, enum state_t state)
{
	if(state == OFF) {
		return OFF;
	}

	const struct vss_device_config* device_config = task->sweep_config->device_config;
	const struct dev_tda18219_priv* priv = device_config->priv;

	unsigned int ch = vss_task_get_channel(task);
	int freq = device_config->channel_base_hz + device_config->channel_spacing_hz * ch;

	if(priv->adc_source == AD8307_SRC_BBAND) {
		freq -= (8000000 - device_config->channel_bw_hz)/2;
	}

	int rssi_dbm_100;
	int r;

	switch(state) {
		case SET_FREQUENCY:
			r = tda18219_set_frequency(priv->standard, freq);
			if(r) {
				vss_task_set_error(task,
						"tda18219_set_frequency() returned an error");
				dev_tda18219_stop();
				return OFF;
			}
			return RUN_MEASUREMENT;

		case RUN_MEASUREMENT:
			r = tda18219_get_input_power_prepare();
			if(r) {
				vss_task_set_error(task,
						"tda18219_get_input_power_prepare() returned an error");
				dev_tda18219_stop();
				return OFF;
			}
			return READ_MEASUREMENT;

		case READ_MEASUREMENT:

			r = get_input_power(&rssi_dbm_100,
					vss_task_get_n_average(task),
					priv->adc_source);
			if(r) {
				vss_task_set_error(task,
						"get_input_power() returned an error");
				dev_tda18219_stop();
				return OFF;
			}

			// extra offset determined by measurement
			int calibration = get_calibration(freq / 1000);

			rssi_dbm_100 -= calibration;

			if(vss_task_insert(task, rssi_dbm_100, vss_rtc_read()) == VSS_OK) {
				return dev_tda18219_state(task, SET_FREQUENCY);
			} else {
				dev_tda18219_stop();
				return OFF;
			}

		default:
			return OFF;
	}
}

void vss_device_tda18219_isr(void)
{
	current_state = dev_tda18219_state(current_task, current_state);
}

int dev_tda18219_run(void* priv __attribute__((unused)), struct vss_task* task)
{
	if(current_task != NULL) {
		return VSS_TOO_MANY;
	}

	int r = dev_tda18219_start(task);
	if(r) return r;

	vss_rtc_reset();

	current_state = dev_tda18219_state(task, SET_FREQUENCY);
	return VSS_OK;
}

static int dev_tda18219_status(void* priv __attribute__((unused)), char* buffer, size_t len)
{
	struct tda18219_status status;
	tda18219_get_status(&status);

	size_t wlen = snprintf(buffer, len,
			"IC          : TDA18219HN\n\n"
			"Ident       : %04x\n"
			"Major rev   : %d\n"
			"Minor rev   : %d\n\n"
			"Temperature : %d C\n"
			"Power-on    : %s\n"
			"LO lock     : %s\n"
			"Sleep mode  : %s\n"
			"Sleep LNA   : %s\n\n",
			status.ident,
			status.major_rev,
			status.minor_rev,
			status.temperature,
			status.por_flag ? "true" : "false",
			status.lo_lock  ? "true" : "false",
			status.sm ? "true" : "false",
			status.sm_lna ? "true" : "false");
	if(wlen >= len) return VSS_TOO_MANY;

	int n;
	size_t r;
	for(n = 0; n < 12; n++) {
		r = snprintf(&buffer[wlen], len - wlen,
				"RF cal %02d   : %d%s\n",
				n, status.calibration_ncaps[n],
				status.calibration_error[n] ? " (error)" : "");
		if(r >= len - wlen) return VSS_TOO_MANY;

		wlen += r;
	}

#ifdef MODEL_SNE_ESHTER
	uint64_t lo, hi;
	int e = vss_eeprom_uid(&lo, &hi);
	if(e) return e;

	r = snprintf(&buffer[wlen], len - wlen,
			"\nBoard UID   : %016llx%016llx\n", hi, lo);
	if(r >= len - wlen) return VSS_TOO_MANY;

	wlen += r;
#endif

	return VSS_OK;
}

static const struct calibration_point* dev_tda18219_get_calibration(
		void* priv __attribute__((unused)),
		const struct vss_device_config* device_config)
{
	const struct dev_tda18219_priv* cpriv = device_config->priv;

	return cpriv->calibration;
}

static int dev_tda18219_baseband(void* priv __attribute__((unused)),
		const struct vss_sweep_config* sweep_config, power_t* buffer, size_t len)
{
	const struct vss_device_config* device_config = sweep_config->device_config;
	const struct dev_tda18219_priv* config_priv = device_config->priv;

	int ch = sweep_config->channel_start;
	int freq = device_config->channel_base_hz + device_config->channel_spacing_hz * ch;

	int r;

	r = dev_tda18219_turn_on(config_priv);
	if(r) return r;

	r = tda18219_set_frequency_sync(config_priv->standard, freq);
	if(r) {
		dev_tda18219_turn_off();
		return VSS_ERROR;
	}

	r = vss_ad8307_get_input_samples((uint16_t*) buffer, len);
	if(r) {
		dev_tda18219_turn_off();
		return VSS_ERROR;
	}

	r = dev_tda18219_turn_off();
	if(r) return r;

	return VSS_OK;
}

static const struct vss_device dev_tda18219 = {
	.name = "tda18219hn",

	.status			= dev_tda18219_status,
	.run			= dev_tda18219_run,
	.get_calibration	= dev_tda18219_get_calibration,
	.baseband		= dev_tda18219_baseband,

	.priv			= NULL
};

static const struct calibration_point dev_tda18219_dvbt_1700khz_calibration[] = {
	{ 470000, 1351 },
	{ 480000, 1387 },
	{ 490000, 1419 },
	{ 500000, 1425 },
	{ 510000, 1423 },
	{ 520000, 1406 },
	{ 530000, 1392 },
	{ 540000, 1382 },
	{ 550000, 1368 },
	{ 560000, 1382 },
	{ 570000, 1325 },
	{ 580000, 1289 },
	{ 590000, 1156 },
	{ 600000, 957 },
	{ 610000, 769 },
	{ 620000, 577 },
	{ 630000, 739 },
	{ 640000, 765 },
	{ 650000, 802 },
	{ 660000, 867 },
	{ 670000, 877 },
	{ 680000, 933 },
	{ 690000, 941 },
	{ 700000, 1007 },
	{ 710000, 1048 },
	{ 720000, 1124 },
	{ 730000, 1142 },
	{ 740000, 1225 },
	{ 750000, 1211 },
	{ 760000, 1251 },
	{ 770000, 1246 },
	{ 780000, 1222 },
	{ 790000, 1250 },
	{ 800000, 1266 },
	{ 810000, 1214 },
	{ 820000, 1096 },
	{ 830000, 929 },
	{ 840000, 769 },
	{ 850000, 623 },
	{ 860000, 433 },
	{ 870000, 242 },
	{ INT_MIN, INT_MIN }
};

static struct dev_tda18219_priv dev_tda18219_dvbt_1700khz_priv = {
	.standard		= &tda18219_standard_dvbt_1700khz,
	.calibration		= dev_tda18219_dvbt_1700khz_calibration,
	.adc_source		= AD8307_SRC_DET,
	.bwsel			= LTC1560_BWSEL_1000KHZ
};

static const struct vss_device_config dev_tda18219_dvbt_1700khz = {
	.name			= "DVB-T 1.7 MHz",

	.device			= &dev_tda18219,

	// UHF: 470 MHz to 862 MHz
	.channel_base_hz 	= 470000000,
	.channel_spacing_hz	= 1000,
	.channel_bw_hz		= 1700000,
	.channel_num		= 392000,

	.channel_time_ms	= 50,

	.priv			= &dev_tda18219_dvbt_1700khz_priv
};

static const struct calibration_point dev_tda18219_dvbt_8000khz_calibration[] = {
	{ 470000, 43 },
	{ 480000, 91 },
	{ 490000, 111 },
	{ 500000, 115 },
	{ 510000, 112 },
	{ 520000, 107 },
	{ 530000, 92 },
	{ 540000, 77 },
	{ 550000, 69 },
	{ 560000, 81 },
	{ 570000, 23 },
	{ 580000, -26 },
	{ 590000, -153 },
	{ 600000, -353 },
	{ 610000, -546 },
	{ 620000, -742 },
	{ 630000, -566 },
	{ 640000, -541 },
	{ 650000, -501 },
	{ 660000, -440 },
	{ 670000, -423 },
	{ 680000, -367 },
	{ 690000, -369 },
	{ 700000, -304 },
	{ 710000, -248 },
	{ 720000, -182 },
	{ 730000, -163 },
	{ 740000, -82 },
	{ 750000, -89 },
	{ 760000, -34 },
	{ 770000, -24 },
	{ 780000, -48 },
	{ 790000, -34 },
	{ 800000, -22 },
	{ 810000, -75 },
	{ 820000, -209 },
	{ 830000, -376 },
	{ 840000, -533 },
	{ 850000, -681 },
	{ 860000, -805 },
	{ 870000, -929 },
	{ INT_MIN, INT_MIN }
};

static struct dev_tda18219_priv dev_tda18219_dvbt_8000khz_priv = {
	.standard		= &tda18219_standard_dvbt_8000khz,
	.calibration		= dev_tda18219_dvbt_8000khz_calibration,
	.adc_source		= AD8307_SRC_DET,
	.bwsel			= LTC1560_BWSEL_1000KHZ
};

static const struct vss_device_config dev_tda18219_dvbt_8000khz = {
	.name			= "DVB-T 8.0 MHz",

	.device			= &dev_tda18219,

	// UHF: 470 MHz to 862 MHz
	.channel_base_hz 	= 470000000,
	.channel_spacing_hz	= 1000,
	.channel_bw_hz		= 8000000,
	.channel_num		= 392000,

	.channel_time_ms	= 50,

	.priv			= &dev_tda18219_dvbt_8000khz_priv
};

static struct dev_tda18219_priv dev_tda18219_dvbt_1000khz_priv = {
	.standard		= &tda18219_standard_dvbt_8000khz,
	.calibration		= dev_tda18219_dvbt_8000khz_calibration,
	.adc_source		= AD8307_SRC_BBAND,
	.bwsel			= LTC1560_BWSEL_1000KHZ
};

static const struct vss_device_config dev_tda18219_dvbt_1000khz = {
	.name			= "DVB-T 1.0 MHz",

	.device			= &dev_tda18219,

	// UHF: 470 MHz to 862 MHz
	.channel_base_hz	= 470000000,
	.channel_spacing_hz	= 1000,
	.channel_bw_hz		= 1000000,
	.channel_num		= 392000,

	.channel_time_ms	= 50,

	.priv			= &dev_tda18219_dvbt_1000khz_priv
};

static struct dev_tda18219_priv dev_tda18219_dvbt_500khz_priv = {
	.standard		= &tda18219_standard_dvbt_8000khz,
	.calibration		= dev_tda18219_dvbt_8000khz_calibration,
	.adc_source		= AD8307_SRC_BBAND,
	.bwsel			= LTC1560_BWSEL_500KHZ
};

static const struct vss_device_config dev_tda18219_dvbt_500khz = {
	.name			= "DVB-T 0.5 MHz",

	.device			= &dev_tda18219,

	// UHF: 470 MHz to 862 MHz
	.channel_base_hz	= 470000000,
	.channel_spacing_hz	= 1000,
	.channel_bw_hz		= 500000,
	.channel_num		= 392000,

	.channel_time_ms	= 50,

	.priv			= &dev_tda18219_dvbt_500khz_priv
};

int vss_device_tda18219_register(void)
{
	int r;
	
	r = vss_device_tda18219_init();
	if(r) return r;

	vss_device_config_add(&dev_tda18219_dvbt_1700khz);
	vss_device_config_add(&dev_tda18219_dvbt_8000khz);
	vss_device_config_add(&dev_tda18219_dvbt_1000khz);
	vss_device_config_add(&dev_tda18219_dvbt_500khz);

	return VSS_OK;
}

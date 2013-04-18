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
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <tda18219/tda18219.h>
#include <tda18219/tda18219regs.h>

#include "ad8307.h"
#include "device.h"
#include "rtc.h"
#include "run.h"
#include "tda18219.h"
#include "vss.h"

#include "device-tda18219.h"

enum state_t {
	OFF,
	RUN_MEASUREMENT,
	READ_MEASUREMENT,
	SET_FREQUENCY };

static enum state_t current_state = OFF;
static struct vss_device_run* current_device_run = NULL;

struct calibration_point {
	unsigned int freq;
	int offset;
};

struct dev_tda18219_priv {
	const struct tda18219_standard* standard;
	const struct calibration_point* calibration;
};

static int get_calibration_offset(const struct calibration_point* calibration, unsigned int freq)
{
	int offset;
	struct calibration_point prev = { 0, 0 };
	struct calibration_point next;

	while(calibration->freq != 0) {
		next = *calibration;

		if(next.freq == freq) {
			offset = next.offset;
			break;
		} else if(next.freq > freq) {
			offset = prev.offset + ((int) (freq - prev.freq)) * (next.offset - prev.offset) / 
					((int) (next.freq - prev.freq));
			break;
		} else {
			prev = next;
		}

		calibration++;
	}

	assert(calibration->freq != 0);

	return offset;
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

	r = vss_ad8307_power_on();
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

enum state_t dev_tda18219_state(struct vss_device_run* device_run, enum state_t state)
{
	if(state == OFF) {
		return OFF;
	}

	const struct vss_device_config* device_config = device_run->sweep_config->device_config;
	const struct dev_tda18219_priv* priv = device_config->priv;

	unsigned int ch = vss_device_run_get_channel(device_run);
	int freq = device_config->channel_base_hz + device_config->channel_spacing_hz * ch;

	uint8_t rssi_dbuv;
	int rssi_dbm_100;
	int r;

	switch(state) {
		case SET_FREQUENCY:
			r = tda18219_set_frequency(priv->standard, freq);
			return RUN_MEASUREMENT;

		case RUN_MEASUREMENT:
			r = tda18219_get_input_power_prepare();
			return READ_MEASUREMENT;

		case READ_MEASUREMENT:

			r = tda18219_get_input_power_read(&rssi_dbuv);
			if(rssi_dbuv < 40) {
				// internal power detector in TDA18219 doesn't go below 40 dBuV
				rssi_dbm_100 = vss_ad8307_get_input_power();
			} else {
				// P [dBm] = U [dBuV] - 90 - 10 log 75 ohm
				rssi_dbm_100 = ((int) rssi_dbuv) * 100 - 9000 - 1875;
			}

			// extra offset determined by measurement
			rssi_dbm_100 -= get_calibration_offset(priv->calibration, freq / 1000);

			if(vss_device_run_insert(device_run, rssi_dbm_100, vss_rtc_read()) == VSS_OK) {
				r = tda18219_set_frequency(priv->standard, freq);
				return RUN_MEASUREMENT;
			} else {
				current_device_run = NULL;
				dev_tda18219_turn_off();
				return OFF;
			}

		default:
			return OFF;
	}
}

void exti9_5_isr(void)
{
	vss_tda18219_irq_ack();
	current_state = dev_tda18219_state(current_device_run, current_state);
}

void exti1_isr(void)
{
	vss_tda18219_irq_ack();
	current_state = dev_tda18219_state(current_device_run, current_state);
}

int dev_tda18219_run(void* priv __attribute__((unused)), struct vss_device_run* device_run)
{
	if(current_device_run != NULL) {
		return VSS_TOO_MANY;
	}
	current_device_run = device_run;

	int r = dev_tda18219_turn_on(device_run->sweep_config->device_config->priv);
	if(r) return r;

	vss_rtc_reset();

	current_state = dev_tda18219_state(device_run, SET_FREQUENCY);
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
	for(n = 0; n < 12; n++) {
		size_t r = snprintf(&buffer[wlen], len - wlen,
				"RF cal %02d   : %d%s\n",
				n, status.calibration_ncaps[n], 
				status.calibration_error[n] ? " (error)" : "");
		if(r >= len - wlen) return VSS_TOO_MANY;

		wlen += r;
	}

	return VSS_OK;
}

static const struct vss_device dev_tda18219 = {
	.name = "tda18219hn",

	.status		= dev_tda18219_status,
	.run		= dev_tda18219_run,

	.priv		= NULL
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
	{ 0, 0 }
};

static struct dev_tda18219_priv dev_tda18219_dvbt_1700khz_priv = {
	.standard		= &tda18219_standard_dvbt_1700khz,
	.calibration		= dev_tda18219_dvbt_1700khz_calibration
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
	{ 0, 0 }
};

static struct dev_tda18219_priv dev_tda18219_dvbt_8000khz_priv = {
	.standard		= &tda18219_standard_dvbt_8000khz,
	.calibration		= dev_tda18219_dvbt_8000khz_calibration
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

int vss_device_tda18219_register(void)
{
	int r;
	
	r = vss_device_tda18219_init();
	if(r) return r;

	vss_device_config_add(&dev_tda18219_dvbt_1700khz);
	vss_device_config_add(&dev_tda18219_dvbt_8000khz);

	return VSS_OK;
}

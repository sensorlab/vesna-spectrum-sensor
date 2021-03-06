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
#ifndef HAVE_SPECTRUM_H
#define HAVE_SPECTRUM_H

struct spectrum_sweep_config;

/* Return 0 to continue sweep, E_SPECTRUM_STOP_SWEEP to stop sweep and return from
 * spectrum_run or any other value on error. */
typedef int (*spectrum_cb_t)(
		/* Pointer to the sweep_config struct passed to spectrum_run */
		const struct spectrum_sweep_config* sweep_config,

		/* Timestamp of the measurement in ms since spectrum_run call
		 *
		 * Note that on a 32-bit architecture this counter allows for
		 * maximum of approx. 25 days of continous measurements. */
		int timestamp,

		/* Array of measurements
		 *
		 * data[n] = measurement for channel m, where
		 *
		 * m = channel_start + channel_step * n
		 *
		 * n = 0 .. channel_num - 1
		 *
		 * Values are input power in 0.01 dBm (e.g. to calculate power in
		 * dBm, divide data[n] by 100)
		 */
		const short int data_list[]);

struct spectrum_sweep_config {
	/* Device configuration Pre-set to use */
	const struct spectrum_dev_config *dev_config;

	/* Channel of the first measurement */
	int channel_start;

	/* Increment in channel number between two measurements */
	int channel_step;

	/* Channel of the one after the last measurement */
	int channel_stop;

	/* Callback function. Return -1 to stop the scan. */
	spectrum_cb_t cb;
};

/* Configuration pre-set for a spectrum sensing device.
 *
 * f_cmin = channel_base
 * f_cmax = channel_base + (channel_num - 1) * channel_spacing
 *
 * resolution_bw = channel_bw
 * sweep_time = channel_time * channel_num */
struct spectrum_dev_config {
	/* Name of the pre-set */
	const char* name;

	/* Center frequency of the first channel in Hz */
	long long int channel_base_hz;

	/* Difference between center frequencies of two adjacent 
	 * channels in Hz */
	int channel_spacing_hz;

	/* Bandwidth of a channel in Hz */
	int channel_bw_hz;

	/* Number of channels */
	int channel_num;

	/* Time required for detection per channel in miliseconds 
	 *
	 * Approximate number - accurate timestamps are returned with
	 * measurement results. */
	int channel_time_ms;

	/* Opaque pointer to a device-specific data structure */
	void* priv;
};

typedef int (*spectrum_dev_reset_t)(void* priv);
typedef int (*spectrum_dev_setup_t)(void* priv, const struct spectrum_sweep_config* sweep_config);
typedef int (*spectrum_dev_run_t)(void* priv, const struct spectrum_sweep_config* sweep_config);

struct spectrum_dev {
	/* Name of the device */
	const char* name;

	/* List of configuration pre-sets supported by this device */
	const struct spectrum_dev_config* const * dev_config_list;
	int dev_config_num;

	/* Reset the device */
	spectrum_dev_reset_t dev_reset;

	/* Setup a spectrum sensing scan */
	spectrum_dev_setup_t dev_setup;

	/* Start a spectrum sensing scan */
	spectrum_dev_run_t dev_run;

	/* Opaque pointer to a device-specific data structure */
	void* priv;
};

#define E_SPECTRUM_STOP_SWEEP 1
#define E_SPECTRUM_OK 0
#define E_SPECTRUM_INVALID -1
#define E_SPECTRUM_TOOMANY -2

#define SPECTRUM_MAX_DEV 10

extern int spectrum_dev_num;
extern const struct spectrum_dev* spectrum_dev_list[];

int spectrum_add_dev(const struct spectrum_dev* dev);
int spectrum_reset(void);
int spectrum_sweep_channel_num(const struct spectrum_sweep_config* sweep_config);
int spectrum_run(const struct spectrum_dev* dev, const struct spectrum_sweep_config* sweep_config);
#endif

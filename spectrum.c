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

/* High-level interface to spectrum sensing hardware */

#include <stdlib.h>
#include "spectrum.h"

int spectrum_dev_num = 0;
const struct spectrum_dev* spectrum_dev_list[SPECTRUM_MAX_DEV];

/* Register a new spectrum sensing device to the system */
int spectrum_add_dev(const struct spectrum_dev* dev)
{
	if(spectrum_dev_num >= SPECTRUM_MAX_DEV) return E_SPECTRUM_TOOMANY;

	spectrum_dev_list[spectrum_dev_num] = dev;
	spectrum_dev_num++;

	return 0;
}

/* Reset all devices 
 *
 * Return 0 on success, or error code otherwise. */
int spectrum_reset(void)
{
	int n;
	for(n = 0; n < spectrum_dev_num; n++) {
		int r = spectrum_dev_list[n]->dev_reset(spectrum_dev_list[n]->priv);
		if(r) return r;
	}

	return 0;
}

/* Return number of channels for a sweep config. */
int spectrum_sweep_channel_num(const struct spectrum_sweep_config* sweep_config)
{
	return (sweep_config->channel_stop - sweep_config->channel_start - 1)
		/ sweep_config->channel_step + 1;
}

/* Start a spectrum sensing on a device 
 *
 * Return 0 on success, or error code otherwise. */
int spectrum_run(const struct spectrum_dev* dev, const struct spectrum_sweep_config* sweep_config)
{
	/* some sanity checks */
	if (sweep_config->channel_start >= sweep_config->channel_stop) {
		return E_SPECTRUM_INVALID;
	}

	if (sweep_config->channel_stop > sweep_config->dev_config->channel_num) {
		return E_SPECTRUM_INVALID;
	}

	if (sweep_config->cb == NULL) {
		return E_SPECTRUM_INVALID;
	}

	int r = dev->dev_setup(dev->priv, sweep_config);
	if(r) return r;

	r = dev->dev_run(dev->priv, sweep_config);
	return r;
}

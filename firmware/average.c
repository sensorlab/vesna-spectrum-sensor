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
#include "average.h"

#include <math.h>

power_t vss_average(power_t* buffer, size_t len)
{
	int acc = 0;

	size_t n;
	for(n = 0; n < len; n++) {
		acc += buffer[n];
	}

	return acc / (int) len;
}

power_t vss_signal_power(uint16_t* buffer, size_t len)
{
	int acc = 0;
	size_t n;
	for(n = 0; n < len; n++) {
		acc += buffer[n];
	}

	double mean = ((double) acc) / len;

	double pacc = 0.;
	for(n = 0; n < len; n++) {
		double m = buffer[n] - mean;
		pacc += m*m;
	}

	if(pacc == 0.) {
		return INT16_MIN;
	} else {
		return 100.*(10.*log10(pacc / len) - 90.31);
	}
}

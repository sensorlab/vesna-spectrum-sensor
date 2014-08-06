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
#include <limits.h>

#include "calibration.h"

/** @brief Obtain a calibration point, interpolating if necessary.
 *
 * Array of calibration points must be terminated with a point that has x and y
 * set to INT_MIN.
 *
 * Returns 0 for points that are out of bounds of the array.
 *
 * @sa calibration_point
 *
 * @param calibration_data pointer to an array of calibration points.
 * @param x independent variable of calibration function.
 * @return value of the calibration function for x. */
int get_calibration(const struct calibration_point* calibration_data, int x)
{
	struct calibration_point prev = { INT_MIN, INT_MIN };
	struct calibration_point next;

	while(calibration_data->x != INT_MIN || calibration_data->y != INT_MIN) {
		next = *calibration_data;

		if(next.x == x) {
			return next.y;
		} else if(next.x > x) {
			if(prev.x == INT_MIN && prev.y == INT_MIN) {
				return 0;
			} else {
				return prev.y + ((int) (x - prev.x)) * (next.y - prev.y) /
						((int) (next.x - prev.x));
			}
		} else {
			prev = next;
		}

		calibration_data++;
	}

	return 0;
}

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

const struct calibration_point const calibration_empty_data[] = {
	{ INT_MIN, INT_MIN }
};

static const struct calibration_point* calibration_cur_data = calibration_empty_data;

/** @brief Set the current calibration table.
 *
 * Array of calibration points must be terminated with a point that has x and y
 * set to INT_MIN.
 *
 * @sa calibration_point
 *
 * @param calibration_data pointer to an array of calibration points. */
void calibration_set_data(const struct calibration_point* calibration_data)
{
	calibration_cur_data = calibration_data;
}

/** @brief Obtain a calibration point, interpolating if necessary.
 *
 * Returns 0 for points that are out of bounds of the array.
 *
 * @sa calibration_set_data
 *
 * @param x independent variable of calibration function.
 * @return value of the calibration function for x. */
int get_calibration(int x)
{
	struct calibration_point prev = { INT_MIN, INT_MIN };
	struct calibration_point next;

	const struct calibration_point* data = calibration_cur_data;

	while(data->x != INT_MIN || data->y != INT_MIN) {
		next = *data;

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

		data++;
	}

	return 0;
}

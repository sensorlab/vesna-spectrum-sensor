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
/** @file
 * @brief Interpolation routines for calibration data */
#ifndef HAVE_CALIBRATION_H
#define HAVE_CALIBRATION_H

/** @brief Single point of calibration. */
struct calibration_point {
	/** @brief Value of the independent variable. */
	int x;
	/** @brief Value of the dependent variable. */
	int y;
};

extern const struct calibration_point const calibration_empty_data[];

void calibration_set_data(const struct calibration_point* calibration_data);
int get_calibration(int x);

#endif

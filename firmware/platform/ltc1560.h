/* Copyright (C) 2014 SensorLab, Jozef Stefan Institute
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
#ifndef HAVE_VSS_LTC1560_H
#define HAVE_VSS_LTC1560_H

#define LTC1560_BWSEL_PIN	GPIO6

#define LTC1560_BWSEL_1000KHZ	1
#define LTC1560_BWSEL_500KHZ	0

int vss_ltc1560_init(void);
int vss_ltc1560_bwsel(int state);

#endif

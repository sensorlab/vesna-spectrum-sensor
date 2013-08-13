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
#include "unity.h"
#include "average.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_average_zeros(void)
{
	int size = 100;
	power_t buffer[size];
	int n;

	for(n = 0; n < size; n++) {
		buffer[n] = 0;
	}

	power_t avg = vss_average(buffer, size);

	TEST_ASSERT_EQUAL(0, avg);
}

void test_average_ramp(void)
{
	int size = 101;
	power_t buffer[size];
	int n;

	for(n = 0; n < size; n++) {
		buffer[n] = n;
	}

	power_t avg = vss_average(buffer, size);

	TEST_ASSERT_EQUAL(50, avg);
}

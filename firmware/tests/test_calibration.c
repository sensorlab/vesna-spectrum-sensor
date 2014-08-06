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

#include "unity.h"
#include "calibration.h"

static const struct calibration_point calibration_data[] = {
	{ 0, 100 },
	{ 100, 200 },
	{ 200, 400 },
	{ INT_MIN, INT_MIN }
};

static const struct calibration_point calibration_empty[] = {
	{ INT_MIN, INT_MIN }
};

void setUp(void)
{
}

void tearDown(void)
{
}

void test_empty(void)
{
	TEST_ASSERT_EQUAL(0, get_calibration(calibration_empty, 10));
}

void test_below_bounds(void)
{
	TEST_ASSERT_EQUAL(0, get_calibration(calibration_data, -10));
}

void test_first_point(void)
{
	TEST_ASSERT_EQUAL(100, get_calibration(calibration_data, 0));
}

void test_middle_point(void)
{
	TEST_ASSERT_EQUAL(200, get_calibration(calibration_data, 100));
}

void test_interpolation(void)
{
	TEST_ASSERT_EQUAL(142, get_calibration(calibration_data, 42));
}


void test_last_point(void)
{
	TEST_ASSERT_EQUAL(400, get_calibration(calibration_data, 200));
}

void test_above_bounds(void)
{
	TEST_ASSERT_EQUAL(0, get_calibration(calibration_data, 201));
}

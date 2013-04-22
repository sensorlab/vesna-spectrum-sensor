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
#include "device.h"

const struct vss_device device_a;

const struct vss_device_config config_a = {
	.device = &device_a
};

const struct vss_device_config config_b = {
	.device = &device_a,
};

void setUp(void)
{
	vss_device_config_add(&config_a);
	vss_device_config_add(&config_b);
}

void tearDown(void)
{
}

void test_config_add(void)
{
	TEST_ASSERT_EQUAL(2, vss_device_config_list_num);
}

void test_config_get(void)
{
	const struct vss_device_config* c;

	c = vss_device_config_get(0, 0);
	TEST_ASSERT_EQUAL(&config_a, c);

	c = vss_device_config_get(0, 1);
	TEST_ASSERT_EQUAL(&config_b, c);
}

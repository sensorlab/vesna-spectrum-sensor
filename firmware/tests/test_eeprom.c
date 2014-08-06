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
#include "unity.h"
#include "vss.h"
#include "eeprom.h"

uint8_t cnt;

void setUp(void)
{
	cnt = 1;
}

void tearDown(void)
{
}

int vss_i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t* value)
{
	*value = cnt;
	cnt++;
	return VSS_OK;
}

void test_uid(void)
{
	uint64_t lo, hi;
	int r;

	r = vss_eeprom_uid(&lo, &hi);

	TEST_ASSERT_EQUAL(VSS_OK, r);

	TEST_ASSERT_EQUAL_HEX64(0x0807060504030201, lo);
	TEST_ASSERT_EQUAL_HEX64(0x100F0E0D0C0B0A09, hi);
}

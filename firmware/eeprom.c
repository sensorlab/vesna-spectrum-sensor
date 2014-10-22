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
#include "eeprom.h"
#include "i2c.h"
#include "vss.h"

int vss_eeprom_uid(uint64_t* lo, uint64_t* hi)
{
	int n;

	*lo = 0;
	for(n = 0; n < 8; n++) {
		uint8_t x;
		vss_i2c_read_reg(EEPROM_I2C_ADDR, UID_BASE + n, &x);
		*lo |= ((uint64_t) x) << (n*8);
	}

	*hi = 0;
	for(n = 0; n < 8; n++) {
		uint8_t x;
		vss_i2c_read_reg(EEPROM_I2C_ADDR, UID_BASE + 8 + n, &x);
		*hi |= ((uint64_t) x) << (n*8);
	}

	return VSS_OK;
}

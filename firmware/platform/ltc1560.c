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
#include "config.h"

#include <libopencm3/stm32/f1/gpio.h>
#include <libopencm3/stm32/f1/rcc.h>

#include "ltc1560.h"
#include "vss.h"

int vss_ltc1560_init(void)
{
#ifdef LTC1560_BWSEL_PIN
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ,
			GPIO_CNF_OUTPUT_PUSHPULL, LTC1560_BWSEL_PIN);
#endif
	return VSS_OK;
}

int vss_ltc1560_bwsel(int state)
{
#ifdef LTC1560_BWSEL_PIN
	if(state) {
		gpio_set(GPIOA, LTC1560_BWSEL_PIN);
	} else {
		gpio_clear(GPIOA, LTC1560_BWSEL_PIN);
	}
#endif

	return VSS_OK;
}

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
#include "config.h"

#include <libopencm3/stm32/f1/gpio.h>

#include "ad8307.h"
#include "vss.h"

int vss_ad8307_init(void)
{
	/* GPIO pin for AD8307 ENB */
#ifdef AD8307_PIN_ENB
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ,
			GPIO_CNF_OUTPUT_PUSHPULL, AD8307_PIN_ENB);
#endif

	return VSS_OK;
}

int vss_ad8307_power_on(void)
{
#ifdef AD8307_PIN_ENB
	gpio_set(GPIOA, AD8307_PIN_ENB);
#endif

	return VSS_OK;
}

int vss_ad8307_power_off(void)
{
#ifdef AD8307_PIN_ENB
	gpio_clear(GPIOA, AD8307_PIN_ENB);
#endif

	return VSS_OK;
}

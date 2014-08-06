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

#include <libopencm3/stm32/exti.h>
#include <libopencm3/stm32/f1/adc.h>
#include <libopencm3/stm32/f1/gpio.h>
#include <libopencm3/stm32/nvic.h>

#include <tda18219/tda18219.h>
#include <tda18219/tda18219regs.h>

#include "i2c.h"
#include "tda18219.h"
#include "vss.h"

#include "device-tda18219.h"

int vss_tda18219_init(void)
{
	int r = vss_i2c_init();
	if(r) return r;

	/* GPIO pin for TDA18219 IRQ */
	gpio_set_mode(GPIOA, GPIO_MODE_INPUT,
			GPIO_CNF_INPUT_FLOAT, TDA_PIN_IRQ);

	exti_select_source(TDA_PIN_IRQ, GPIOA);
	exti_set_trigger(TDA_PIN_IRQ, EXTI_TRIGGER_RISING);
	exti_enable_request(TDA_PIN_IRQ);

	uint8_t irqn;
	if(TDA_PIN_IRQ == GPIO1) {
		irqn = NVIC_EXTI1_IRQ;
	} else if(TDA_PIN_IRQ == GPIO7) {
		irqn = NVIC_EXTI9_5_IRQ;
	}

	nvic_enable_irq(irqn);
	nvic_set_priority(irqn, 255);

	/* GPIO pin for TDA18219 IF AGC */
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ,
			GPIO_CNF_OUTPUT_PUSHPULL, TDA_PIN_IF_AGC);
	/* Set to lowest gain for now */
	gpio_clear(GPIOA, TDA_PIN_IF_AGC);

	return VSS_OK;
}

void exti1_isr(void)
{
	exti_reset_request(TDA_PIN_IRQ);
	vss_device_tda18219_isr();
}

void exti9_5_isr(void)
{
	exti_reset_request(TDA_PIN_IRQ);
	vss_device_tda18219_isr();
}

int tda18219_read_reg(uint8_t reg, uint8_t* value)
{
	return vss_i2c_read_reg(TDA18219_I2C_ADDR, reg, value);
}

int tda18219_write_reg(uint8_t reg, uint8_t value)
{
	return vss_i2c_write_reg(TDA18219_I2C_ADDR, reg, value);
}

int tda18219_wait_irq(void)
{
	while(!gpio_get(GPIOA, TDA_PIN_IRQ));

	return 0;
}

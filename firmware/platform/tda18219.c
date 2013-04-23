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
#include <libopencm3/stm32/exti.h>
#include <libopencm3/stm32/f1/adc.h>
#include <libopencm3/stm32/f1/gpio.h>
#include <libopencm3/stm32/f1/rcc.h>
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/nvic.h>

#include <tda18219/tda18219.h>
#include <tda18219/tda18219regs.h>

#include "tda18219.h"
#include "vss.h"

#include "device-tda18219.h"

int vss_tda18219_init(void)
{
	rcc_peripheral_enable_clock(&RCC_APB1ENR, 
			RCC_APB1ENR_I2C1EN);

	/* GPIO pin for I2C1 SCL, SDA */

	/* VESNA v1.0
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,
			GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN, GPIO6);
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,
			GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN, GPIO7);
	*/

	/* VESNA v1.1 */
	AFIO_MAPR |= AFIO_MAPR_I2C1_REMAP;
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,
			GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN, TDA_PIN_SCL);
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,
			GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN, TDA_PIN_SDA);

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

	/* Setup I2C */
	i2c_peripheral_disable(I2C1);

	/* 400 kHz - I2C Fast Mode */
	i2c_set_clock_frequency(I2C1, I2C_CR2_FREQ_24MHZ);
	i2c_set_fast_mode(I2C1);
	/* 400 kHz */
	i2c_set_ccr(I2C1, 0x14);
	/* 300 ns rise time */
	i2c_set_trise(I2C1, 0x08);

	i2c_peripheral_enable(I2C1);

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
	uint32_t __attribute__((unused)) reg32;

	/* Send START condition. */
	i2c_send_start(I2C1);

	/* Waiting for START is send and switched to master mode. */
	while (!((I2C_SR1(I2C1) & I2C_SR1_SB)
		& (I2C_SR2(I2C1) & (I2C_SR2_MSL | I2C_SR2_BUSY))));

	/* Say to what address we want to talk to. */
	/* Yes, WRITE is correct - for selecting register in STTS75. */
	i2c_send_7bit_address(I2C1, TDA18219_I2C_ADDR, I2C_WRITE);

	/* Waiting for address is transferred. */
	while (!(I2C_SR1(I2C1) & I2C_SR1_ADDR));

	/* Cleaning ADDR condition sequence. */
	reg32 = I2C_SR2(I2C1);

	i2c_send_data(I2C1, reg);
	while (!(I2C_SR1(I2C1) & (I2C_SR1_BTF | I2C_SR1_TxE)));

	/* Send re-START condition. */
	i2c_send_start(I2C1);

	/* Waiting for START is send and switched to master mode. */
	while (!((I2C_SR1(I2C1) & I2C_SR1_SB)
		& (I2C_SR2(I2C1) & (I2C_SR2_MSL | I2C_SR2_BUSY))));

	/* Say to what address we want to talk to. */
	i2c_send_7bit_address(I2C1, TDA18219_I2C_ADDR, I2C_READ); 

	I2C_CR1(I2C1) &= ~I2C_CR1_ACK;

	/* Waiting for address is transferred. */
	while (!(I2C_SR1(I2C1) & I2C_SR1_ADDR));

	/* Cleaning ADDR condition sequence. */
	reg32 = I2C_SR2(I2C1);

	i2c_send_stop(I2C1);

	while (!(I2C_SR1(I2C1) & I2C_SR1_RxNE));

	*value = I2C_DR(I2C1);

	return 0;
}

int tda18219_write_reg(uint8_t reg, uint8_t value)
{
	uint32_t __attribute__((unused)) reg32;

	/* Send START condition. */
	i2c_send_start(I2C1);

	/* Waiting for START is send and switched to master mode. */
	while (!((I2C_SR1(I2C1) & I2C_SR1_SB)
		& (I2C_SR2(I2C1) & (I2C_SR2_MSL | I2C_SR2_BUSY))));

	/* Say to what address we want to talk to. */
	/* Yes, WRITE is correct - for selecting register in STTS75. */
	i2c_send_7bit_address(I2C1, TDA18219_I2C_ADDR, I2C_WRITE);

	/* Waiting for address is transferred. */
	while (!(I2C_SR1(I2C1) & I2C_SR1_ADDR));

	/* Cleaning ADDR condition sequence. */
	reg32 = I2C_SR2(I2C1);

	i2c_send_data(I2C1, reg);
	while (!(I2C_SR1(I2C1) & I2C_SR1_TxE));

	i2c_send_data(I2C1, value);
	while (!(I2C_SR1(I2C1) & (I2C_SR1_BTF | I2C_SR1_TxE)));

	i2c_send_stop(I2C1);

	return 0;
}

int tda18219_wait_irq(void)
{
	while(!gpio_get(GPIOA, TDA_PIN_IRQ));

	return 0;
}

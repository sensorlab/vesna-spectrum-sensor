/* Copyright (C) 2012 SensorLab, Jozef Stefan Institute
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
#include <stdio.h>
#include <stdlib.h>
#include <libopencm3/stm32/f1/adc.h>
#include <libopencm3/stm32/f1/dma.h>
#include <libopencm3/stm32/f1/gpio.h>
#include <libopencm3/stm32/f1/rcc.h>
#include <libopencm3/stm32/f1/rtc.h>
#include <libopencm3/stm32/i2c.h>
#include <tda18219/tda18219.h>
#include <tda18219/tda18219regs.h>

#include "dev-tda18219.h"
#include "spectrum.h"

uint16_t buff[5000];

static void setup_stm32f1_peripherals(void)
{
	rcc_peripheral_enable_clock(&RCC_APB1ENR, 
			RCC_APB1ENR_I2C1EN);

	rcc_peripheral_enable_clock(&RCC_APB2ENR, 
			RCC_APB2ENR_ADC1EN);

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

	/* GPIO pin for TDA18219 IF AGC */
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ,
			GPIO_CNF_OUTPUT_PUSHPULL, TDA_PIN_IF_AGC);
	/* Set to lowest gain for now */
	gpio_clear(GPIOA, TDA_PIN_IF_AGC);

	/* GPIO pin for AD8307 ENB */
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ,
			GPIO_CNF_OUTPUT_PUSHPULL, TDA_PIN_ENB);

	/* ADC pin for AD8307 output */
	gpio_set_mode(GPIOA, GPIO_MODE_INPUT,
			GPIO_CNF_INPUT_ANALOG, TDA_PIN_OUT);

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


	/* setup DMA */
	rcc_peripheral_enable_clock(&RCC_AHBENR,
			RCC_AHBENR_DMA1EN);

	/* Make sure the ADC doesn't run during config. */
	adc_off(ADC1);

	dma_channel_reset(DMA1, DMA_CHANNEL1);
	dma_set_peripheral_address(DMA1, DMA_CHANNEL1, (u32) &ADC1_DR);
	dma_set_memory_address(DMA1, DMA_CHANNEL1, (u32) &buff);
	dma_set_priority(DMA1, DMA_CHANNEL1, DMA_CCR_PL_VERY_HIGH);
	dma_set_peripheral_size(DMA1, DMA_CHANNEL1, DMA_CCR_PSIZE_16BIT);
	dma_set_memory_size(DMA1, DMA_CHANNEL1, DMA_CCR_MSIZE_16BIT);
	dma_enable_memory_increment_mode(DMA1, DMA_CHANNEL1);
	dma_set_read_from_peripheral(DMA1, DMA_CHANNEL1);

	/* We configure everything for one single conversion. */
	adc_enable_scan_mode(ADC1);
	adc_set_continous_conversion_mode(ADC1);
	adc_disable_external_trigger_regular(ADC1);
	adc_set_right_aligned(ADC1);
	adc_set_conversion_time_on_all_channels(ADC1, ADC_SMPR_SMP_1DOT5CYC);
	adc_enable_dma(ADC1);

	adc_on(ADC1);

	/* Wait for ADC starting up. */
	int i;
	for (i = 0; i < 800000; i++)    /* Wait a bit. */
		__asm__("nop");

	adc_reset_calibration(ADC1);
	adc_calibration(ADC1);

	uint8_t channel_array[16];
	/* Select the channel we want to convert. */
	if(TDA_PIN_OUT == GPIO0) {
		channel_array[0] = 0;
	} else if(TDA_PIN_OUT == GPIO2) {
		channel_array[0] = 2;
	}
	adc_set_regular_sequence(ADC1, 1, channel_array);
}


uint8_t tda18219_read_reg(uint8_t reg)
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

	uint8_t value = I2C_DR(I2C1);
	return value;
}

void tda18219_write_reg(uint8_t reg, uint8_t value)
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
}

void tda18219_wait_irq(void)
{
	while(!gpio_get(GPIOA, TDA_PIN_IRQ));
}


int dev_tda18219_reset(void* priv __attribute__((unused))) 
{
	setup_stm32f1_peripherals();
	tda18219_power_on();
	tda18219_init();
	tda18219_power_standby();
	return E_SPECTRUM_OK;
}

int dev_tda18219_setup(void* priv __attribute__((unused)), const struct spectrum_sweep_config* sweep_config) 
{
	tda18219_power_on();
	tda18219_set_standard((struct tda18219_standard*) sweep_config->dev_config->priv);
	tda18219_power_standby();
	return E_SPECTRUM_OK;
}

static void dev_tda18219_get_ad8307_input_power(void)
{
	const int nsamples = sizeof(buff)/sizeof(*buff);

	dma_set_number_of_data(DMA1, DMA_CHANNEL1, sizeof(buff)/sizeof(*buff));
	dma_enable_channel(DMA1, DMA_CHANNEL1);

	adc_on(ADC1);
	while(!(DMA_ISR(DMA1) & DMA_ISR_TCIF(DMA_CHANNEL1))) {}
	DMA_IFCR(DMA1) = DMA_IFCR_CTCIF(DMA_CHANNEL1);

	dma_disable_channel(DMA1, DMA_CHANNEL1);

	struct tda18219_status status;
	tda18219_get_status(&status);

	int n;
	printf("TS 0.0 TE %d DS", status.temperature);
	for(n = 0; n < nsamples; n++) {
		printf(" %d", buff[n]);
	}
	printf(" DE\n");
}

int dev_tda18219_run(void* priv __attribute__((unused)), const struct spectrum_sweep_config* sweep_config)
{
	int r;
	short int *data;

	rtc_set_counter_val(0);

	int channel_num = spectrum_sweep_channel_num(sweep_config);
	data = calloc(channel_num, sizeof(*data));
	if (data == NULL) {
		return E_SPECTRUM_TOOMANY;
	}

	tda18219_power_on();
	gpio_set(GPIOA, TDA_PIN_ENB);

	do {
		uint32_t rtc_counter = rtc_get_counter_val();
		/* LSE clock is 32768 Hz. Prescaler is set to 16.
		 *
		 *                 rtc_counter * 16
		 * t [ms] = 1000 * ----------------
		 *                       32768
		 */
		int timestamp = ((long long) rtc_counter) * 1000 / 2048;

		int ch = sweep_config->channel_start;

		int freq = sweep_config->dev_config->channel_base_hz + \
				sweep_config->dev_config->channel_spacing_hz * ch;

		tda18219_set_frequency((struct tda18219_standard*) sweep_config->dev_config->priv,
				freq);

		dev_tda18219_get_ad8307_input_power();

		r = sweep_config->cb(sweep_config, timestamp, data);
	} while(!r);

	gpio_clear(GPIOA, TDA_PIN_ENB);
	tda18219_power_standby();

	free(data);

	if (r == E_SPECTRUM_STOP_SWEEP) {
		return E_SPECTRUM_OK;
	} else {
		return r;
	}
}

void dev_tda18219_print_status(void)
{
	struct tda18219_status status;
	tda18219_get_status(&status);

	printf("IC          : TDA18219HN\n\n");
	printf("Ident       : %04x\n", status.ident);
	printf("Major rev   : %d\n", status.major_rev);
	printf("Minor rev   : %d\n\n", status.minor_rev);
	printf("Temperature : %d C\n", status.temperature);
	printf("Power-on    : %s\n", status.por_flag ? "true" : "false");
	printf("LO lock     : %s\n", status.lo_lock  ? "true" : "false");
	printf("Sleep mode  : %s\n", status.sm ? "true" : "false");
	printf("Sleep LNA   : %s\n\n", status.sm_lna ? "true" : "false");

	int n;
	for(n = 0; n < 12; n++) {
		printf("RF cal %02d   : %d%s\n", n, status.calibration_ncaps[n], 
				status.calibration_error[n] ? " (error)" : "");
	}
}

const struct spectrum_dev_config dev_tda18219_dvbt_1700khz = {
	.name			= "DVB-T 1.7 MHz",

	// UHF: 470 MHz to 862 MHz
	.channel_base_hz 	= 470000000,
	.channel_spacing_hz	= 1000,
	.channel_bw_hz		= 1700000,
	.channel_num		= 392000,

	.channel_time_ms	= 50,

	.priv			= &tda18219_standard_dvbt_1700khz
};

const struct spectrum_dev_config* dev_tda18219_config_list[] = { &dev_tda18219_dvbt_1700khz };

const struct spectrum_dev dev_tda18219 = {
	.name = "tda18219hn",

	.dev_config_list	= dev_tda18219_config_list,
	.dev_config_num		= 1,

	.dev_reset		= dev_tda18219_reset,
	.dev_setup		= dev_tda18219_setup,
	.dev_run		= dev_tda18219_run,

	.priv 			= NULL
};

int dev_tda18219_register(void)
{
	return spectrum_add_dev(&dev_tda18219);
}

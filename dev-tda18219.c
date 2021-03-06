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
#include <assert.h>
#include <libopencm3/stm32/f1/adc.h>
#include <libopencm3/stm32/f1/gpio.h>
#include <libopencm3/stm32/f1/rcc.h>
#include <libopencm3/stm32/f1/rtc.h>
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/iwdg.h>
#include <tda18219/tda18219.h>
#include <tda18219/tda18219regs.h>

#include "dev-tda18219.h"
#include "spectrum.h"

struct calibration_point {
	unsigned int freq;
	int offset;
};

struct dev_tda18219_priv {
	const struct tda18219_standard* standard;
	const struct calibration_point* calibration;
};

static int get_calibration_offset(const struct calibration_point* calibration, unsigned int freq)
{
	int offset;
	struct calibration_point prev = { 0, 0 };
	struct calibration_point next;

	while(calibration->freq != 0) {
		next = *calibration;

		if(next.freq == freq) {
			offset = next.offset;
			break;
		} else if(next.freq > freq) {
			offset = prev.offset + ((int) (freq - prev.freq))* (next.offset - prev.offset) / ((int) (next.freq - prev.freq));
			break;
		} else {
			prev = next;
		}

		calibration++;
	}

	assert(calibration->freq != 0);

	return offset;
}

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


	/* Make sure the ADC doesn't run during config. */
	adc_off(ADC1);

	/* We configure everything for one single conversion. */
	adc_disable_scan_mode(ADC1);
	adc_set_single_conversion_mode(ADC1);
	adc_enable_discontinous_mode_regular(ADC1);
	adc_disable_external_trigger_regular(ADC1);
	adc_set_right_aligned(ADC1);
	adc_set_conversion_time_on_all_channels(ADC1, ADC_SMPR_SMP_28DOT5CYC);

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
	const struct dev_tda18219_priv* dev_priv = sweep_config->dev_config->priv;

	tda18219_power_on();
	tda18219_set_standard(dev_priv->standard);
	tda18219_power_standby();
	return E_SPECTRUM_OK;
}

static int dev_tda18219_get_ad8307_input_power(void)
{
	const int nsamples = 100;

	int acc = 0;
	int n;
	for(n = 0; n < nsamples; n++) {
		adc_on(ADC1);
		while (!(ADC_SR(ADC1) & ADC_SR_EOC));
		acc += ADC_DR(ADC1);
	}
	acc /= nsamples;

	/* STM32F1 has a 12 bit AD converter. Low reference is 0 V, high is 3.3 V
	 *
	 *              3.3 V
	 *     Kad = ----------
	 *           (2^12 - 1)
	 *
	 * AD8307
	 *
	 *     Kdet = 25 mV/dB  (slope)
	 *     Adet = -84 dBm   (intercept)
	 *
	 * Since we are using this detector for low power signals only, TDA18219 is
	 * at maximum gain.
	 *
	 *     AGC1 = 15 dB
	 *     AGC2 = -2 dB
	 *     AGC3 = 30 dB (guess based on measurement)
	 *     AGC4 = 14 dB
	 *     AGC5 = 9 dB
	 *     --------------
	 *     Atuner = 66 dB
	 *
	 * Pinput [dBm] = N * Kad / Kdet - Adet - Atuner
	 *
	 *                  3.3 V * 1000
	 *              = N ------------ - 84 - 66
	 *                  4095 * 25 V
	 *
	 * Note we are returning [dBm * 100]
	 */
	return acc * 3300 / 1024 - 15000;
}

int dev_tda18219_run(void* priv __attribute__((unused)), const struct spectrum_sweep_config* sweep_config)
{
	const struct dev_tda18219_priv* dev_priv = sweep_config->dev_config->priv;

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
		IWDG_KR = IWDG_KR_RESET;
		uint32_t rtc_counter = rtc_get_counter_val();
		/* LSE clock is 32768 Hz. Prescaler is set to 16.
		 *
		 *                 rtc_counter * 16
		 * t [ms] = 1000 * ----------------
		 *                       32768
		 */
		int timestamp = ((long long) rtc_counter) * 1000 / 2048;

		int n, ch;
		for(		ch = sweep_config->channel_start, n = 0; 
				ch < sweep_config->channel_stop && n < channel_num; 
				ch += sweep_config->channel_step, n++) {
			int freq = sweep_config->dev_config->channel_base_hz + \
				   	sweep_config->dev_config->channel_spacing_hz * ch;
			tda18219_set_frequency(dev_priv->standard,
					freq);

			uint8_t rssi_dbuv;
			tda18219_get_input_power_sync(&rssi_dbuv);

			int rssi_dbm_100;
			if(rssi_dbuv < 40) {
				// internal power detector in TDA18219 doesn't go below 40 dBuV
				rssi_dbm_100 = dev_tda18219_get_ad8307_input_power();
			} else {
				// P [dBm] = U [dBuV] - 90 - 10 log 75 ohm
				rssi_dbm_100 = rssi_dbuv * 100 - 9000 - 1875;
			}

			// extra offset determined by measurement
			rssi_dbm_100 -= get_calibration_offset(dev_priv->calibration, freq / 1000);

			data[n] = rssi_dbm_100;
		}
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

static const struct calibration_point dev_tda18219_dvbt_1700khz_calibration[] = {
	{ 470000, 1351 },
	{ 480000, 1387 },
	{ 490000, 1419 },
	{ 500000, 1425 },
	{ 510000, 1423 },
	{ 520000, 1406 },
	{ 530000, 1392 },
	{ 540000, 1382 },
	{ 550000, 1368 },
	{ 560000, 1382 },
	{ 570000, 1325 },
	{ 580000, 1289 },
	{ 590000, 1156 },
	{ 600000, 957 },
	{ 610000, 769 },
	{ 620000, 577 },
	{ 630000, 739 },
	{ 640000, 765 },
	{ 650000, 802 },
	{ 660000, 867 },
	{ 670000, 877 },
	{ 680000, 933 },
	{ 690000, 941 },
	{ 700000, 1007 },
	{ 710000, 1048 },
	{ 720000, 1124 },
	{ 730000, 1142 },
	{ 740000, 1225 },
	{ 750000, 1211 },
	{ 760000, 1251 },
	{ 770000, 1246 },
	{ 780000, 1222 },
	{ 790000, 1250 },
	{ 800000, 1266 },
	{ 810000, 1214 },
	{ 820000, 1096 },
	{ 830000, 929 },
	{ 840000, 769 },
	{ 850000, 623 },
	{ 860000, 433 },
	{ 870000, 242 },
	{ 0, 0 }
};

static struct dev_tda18219_priv dev_tda18219_dvbt_1700khz_priv = {
	.standard		= &tda18219_standard_dvbt_1700khz,
	.calibration		= dev_tda18219_dvbt_1700khz_calibration
};

static const struct spectrum_dev_config dev_tda18219_dvbt_1700khz = {
	.name			= "DVB-T 1.7 MHz",

	// UHF: 470 MHz to 862 MHz
	.channel_base_hz 	= 470000000,
	.channel_spacing_hz	= 1000,
	.channel_bw_hz		= 1700000,
	.channel_num		= 392000,

	.channel_time_ms	= 50,

	.priv			= &dev_tda18219_dvbt_1700khz_priv
};

static const struct calibration_point dev_tda18219_dvbt_8000khz_calibration[] = {
	{ 470000, 43 },
	{ 480000, 91 },
	{ 490000, 111 },
	{ 500000, 115 },
	{ 510000, 112 },
	{ 520000, 107 },
	{ 530000, 92 },
	{ 540000, 77 },
	{ 550000, 69 },
	{ 560000, 81 },
	{ 570000, 23 },
	{ 580000, -26 },
	{ 590000, -153 },
	{ 600000, -353 },
	{ 610000, -546 },
	{ 620000, -742 },
	{ 630000, -566 },
	{ 640000, -541 },
	{ 650000, -501 },
	{ 660000, -440 },
	{ 670000, -423 },
	{ 680000, -367 },
	{ 690000, -369 },
	{ 700000, -304 },
	{ 710000, -248 },
	{ 720000, -182 },
	{ 730000, -163 },
	{ 740000, -82 },
	{ 750000, -89 },
	{ 760000, -34 },
	{ 770000, -24 },
	{ 780000, -48 },
	{ 790000, -34 },
	{ 800000, -22 },
	{ 810000, -75 },
	{ 820000, -209 },
	{ 830000, -376 },
	{ 840000, -533 },
	{ 850000, -681 },
	{ 860000, -805 },
	{ 870000, -929 },
	{ 0, 0 }
};

static struct dev_tda18219_priv dev_tda18219_dvbt_8000khz_priv = {
	.standard		= &tda18219_standard_dvbt_8000khz,
	.calibration		= dev_tda18219_dvbt_8000khz_calibration
};

static const struct spectrum_dev_config dev_tda18219_dvbt_8000khz = {
	.name			= "DVB-T 8.0 MHz",

	// UHF: 470 MHz to 862 MHz
	.channel_base_hz 	= 470000000,
	.channel_spacing_hz	= 1000,
	.channel_bw_hz		= 8000000,
	.channel_num		= 392000,

	.channel_time_ms	= 50,

	.priv			= &dev_tda18219_dvbt_8000khz_priv
};

static const struct spectrum_dev_config* dev_tda18219_config_list[] = {
	&dev_tda18219_dvbt_1700khz,
	&dev_tda18219_dvbt_8000khz
};

static const struct spectrum_dev dev_tda18219 = {
	.name = "tda18219hn",

	.dev_config_list	= dev_tda18219_config_list,
	.dev_config_num		= sizeof(dev_tda18219_config_list)/sizeof(struct spectrum_dev_config*),

	.dev_reset		= dev_tda18219_reset,
	.dev_setup		= dev_tda18219_setup,
	.dev_run		= dev_tda18219_run,

	.priv 			= NULL
};

int dev_tda18219_register(void)
{
	return spectrum_add_dev(&dev_tda18219);
}

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
#include <libopencm3/stm32/f1/adc.h>
#include <libopencm3/stm32/f1/gpio.h>
#include <libopencm3/stm32/f1/rcc.h>

#include "ad8307.h"
#include "vss.h"

int vss_ad8307_init(void)
{
	rcc_peripheral_enable_clock(&RCC_APB2ENR, 
			RCC_APB2ENR_ADC1EN);

	/* GPIO pin for AD8307 ENB */
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ,
			GPIO_CNF_OUTPUT_PUSHPULL, AD8307_PIN_ENB);

	/* ADC pin for AD8307 output */
	gpio_set_mode(GPIOA, GPIO_MODE_INPUT,
			GPIO_CNF_INPUT_ANALOG, AD8307_PIN_OUT);

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
	if(AD8307_PIN_OUT == GPIO0) {
		channel_array[0] = 0;
	} else if(AD8307_PIN_OUT == GPIO2) {
		channel_array[0] = 2;
	}
	adc_set_regular_sequence(ADC1, 1, channel_array);

	return VSS_OK;
}

int vss_ad8307_power_on(void)
{
	gpio_set(GPIOA, AD8307_PIN_ENB);
	return VSS_OK;
}

int vss_ad8307_power_off(void)
{
	gpio_clear(GPIOA, AD8307_PIN_ENB);
	return VSS_OK;
}

int vss_ad8307_get_input_samples(unsigned* buffer, unsigned nsamples)
{
	unsigned n;
	for(n = 0; n < nsamples; n++) {
		adc_on(ADC1);
		while (!(ADC_SR(ADC1) & ADC_SR_EOC));
		buffer[n] = ADC_DR(ADC1);
	}

	return VSS_OK;
}

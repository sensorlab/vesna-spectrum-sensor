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

#include <libopencm3/stm32/f1/adc.h>
#include <libopencm3/stm32/f1/dma.h>
#include <libopencm3/stm32/f1/gpio.h>
#include <libopencm3/stm32/f1/rcc.h>

#include "adc.h"
#include "vss.h"

int vss_adc_init(void)
{
	rcc_peripheral_enable_clock(&RCC_APB2ENR,
			RCC_APB2ENR_ADC1EN);

	rcc_peripheral_enable_clock(&RCC_AHBENR,
			RCC_AHBENR_DMA1EN);

	/* Pins for ADC input */
	gpio_set_mode(GPIOA, GPIO_MODE_INPUT,
			GPIO_CNF_INPUT_ANALOG, ADC_DET_PIN);

#ifdef ADC_BBAND_PIN
	gpio_set_mode(GPIOA, GPIO_MODE_INPUT,
			GPIO_CNF_INPUT_ANALOG, ADC_BBAND_PIN);
#endif

	return VSS_OK;
}

static int vss_adc_setup(uint8_t conv_time, uint32_t adcpre, uint16_t pin)
{
	rcc_set_adcpre(adcpre);

	/* Make sure the ADC doesn't run during config. */
	adc_off(ADC1);

	dma_channel_reset(DMA1, DMA_CHANNEL1);
	dma_set_peripheral_address(DMA1, DMA_CHANNEL1, (u32) &ADC1_DR);
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
	adc_set_conversion_time_on_all_channels(ADC1, conv_time);
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
	if(pin == GPIO0) {
		channel_array[0] = 0;
	} else if(pin == GPIO2) {
		channel_array[0] = 2;
	} else {
		return VSS_ERROR;
	}
	adc_set_regular_sequence(ADC1, 1, channel_array);

	return VSS_OK;
}

int vss_adc_power_on(int src)
{
	switch(src) {
#ifdef ADC_BBAND_PIN
		case ADC_SRC_BBAND:
			vss_adc_setup(ADC_SMPR_SMP_1DOT5CYC,
					RCC_CFGR_ADCPRE_PCLK2_DIV4,
					ADC_BBAND_PIN);
			break;
#endif
		case ADC_SRC_DET:
			vss_adc_setup(ADC_SMPR_SMP_28DOT5CYC,
					RCC_CFGR_ADCPRE_PCLK2_DIV8,
					ADC_DET_PIN);
			break;
		default:
			return VSS_ERROR;
	}

	return VSS_OK;
}

int vss_adc_power_off(void)
{
	adc_off(ADC1);
	return VSS_OK;
}

int vss_adc_get_input_samples(uint16_t* buffer, unsigned nsamples)
{
	dma_set_memory_address(DMA1, DMA_CHANNEL1, (u32) buffer);
	dma_set_number_of_data(DMA1, DMA_CHANNEL1, nsamples);
	dma_enable_channel(DMA1, DMA_CHANNEL1);

	adc_on(ADC1);

	while(!(DMA_ISR(DMA1) & DMA_ISR_TCIF(DMA_CHANNEL1))) {}
	DMA_IFCR(DMA1) = DMA_IFCR_CTCIF(DMA_CHANNEL1);

	dma_disable_channel(DMA1, DMA_CHANNEL1);

	return VSS_OK;
}

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
#ifndef HAVE_VSS_ADC_H
#define HAVE_VSS_ADC_H

#include <stdint.h>

#ifdef MODEL_SNE_CREWTV
#	define ADC_DET_PIN	GPIO0
#endif

#ifdef MODEL_SNE_ISMTV_UHF
#	define ADC_DET_PIN	GPIO2
#endif

#ifdef MODEL_SNE_ESHTER
#	define ADC_DET_PIN	GPIO0
#	define ADC_BBAND_PIN	GPIO2
#endif

#define ADC_SRC_DET		0
#define ADC_SRC_BBAND		1
#define ADC_SRC_BBAND_DUAL	2

int vss_adc_init(void);
int vss_adc_power_on(int src);
int vss_adc_power_off(void);
int vss_adc_get_input_samples(uint16_t* buffer, unsigned nsamples);

#endif

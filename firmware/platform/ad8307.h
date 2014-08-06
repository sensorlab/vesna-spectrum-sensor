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
#ifndef HAVE_AD8307_H
#define HAVE_AD8307_H

#ifdef MODEL_SNE_CREWTV
#	define AD8307_PIN_ENB	GPIO6
#	define AD8307_PIN_OUT	GPIO0
#endif

#ifdef MODEL_SNE_ISMTV_UHF
#	define AD8307_PIN_ENB	GPIO0
#	define AD8307_PIN_OUT	GPIO2
#endif

#ifdef MODEL_SNE_ESHTER
#	define AD8307_PIN_OUT	GPIO0
#endif

int vss_ad8307_init(void);
int vss_ad8307_power_on(void);
int vss_ad8307_power_off(void);
int vss_ad8307_get_input_samples(unsigned* buffer, unsigned nsamples);

#endif

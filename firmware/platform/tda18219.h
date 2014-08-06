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
#ifndef HAVE_VSS_TDA18219_H
#define HAVE_VSS_TDA18219_H

#include <stdint.h>

#define TDA_PIN_SCL	GPIO8
#define TDA_PIN_SDA	GPIO9

#ifdef MODEL_SNE_CREWTV
#	define TDA_PIN_IRQ	GPIO7
#	define TDA_PIN_IF_AGC	GPIO4
#	define TDA_PIN_ENB	GPIO6
#endif

#ifdef MODEL_SNE_ISMTV_UHF
#	define TDA_PIN_IRQ	GPIO1
#	define TDA_PIN_IF_AGC	GPIO4
#	define TDA_PIN_ENB	GPIO0
#endif

#ifdef MODEL_SNE_ESHTER
#	define TDA_PIN_IRQ	GPIO7
#	define TDA_PIN_IF_AGC	GPIO4
#endif

int vss_tda18219_init(void);
void vss_tda18219_irq_ack(void);

#endif

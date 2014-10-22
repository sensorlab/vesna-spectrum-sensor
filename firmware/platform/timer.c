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
#include <libopencm3/stm32/f1/rcc.h>
#include <libopencm3/stm32/nvic.h>
#include <libopencm3/stm32/timer.h>

#include "vss.h"
#include "timer.h"

int vss_timer_init(void)
{
	rcc_peripheral_enable_clock(&RCC_APB1ENR, RCC_APB1ENR_TIM4EN);

	nvic_enable_irq(NVIC_TIM4_IRQ);
	nvic_set_priority(NVIC_TIM4_IRQ, 255);

	timer_reset(TIM4);
	timer_set_mode(TIM4, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
	timer_set_prescaler(TIM4, 48000);
	timer_one_shot_mode(TIM4);

	timer_generate_event(TIM4, TIM_EGR_UG);
	timer_clear_flag(TIM4, TIM_SR_UIF);

	timer_enable_irq(TIM4, TIM_DIER_UIE);

	return VSS_OK;
}

int vss_timer_schedule(uint32_t delay_ms)
{
	timer_set_period(TIM4, delay_ms);
	timer_enable_counter(TIM4);

	return VSS_OK;
}

void vss_timer_ack(void)
{
	timer_clear_flag(TIM4, TIM_SR_UIF);
}

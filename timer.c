#include <libopencm3/stm32/f1/rcc.h>
#include <libopencm3/stm32/nvic.h>
#include <libopencm3/stm32/timer.h>

#include "vss.h"
#include "timer.h"

int vss_timer_init(void)
{
	rcc_peripheral_enable_clock(&RCC_APB1ENR, RCC_APB1ENR_TIM4EN);

	nvic_enable_irq(NVIC_TIM4_IRQ);
	nvic_set_priority(NVIC_TIM4_IRQ, 2);

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

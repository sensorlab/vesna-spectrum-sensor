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

int vss_tda18219_init(void);
void vss_tda18219_irq_ack(void);

#endif

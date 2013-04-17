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
uint8_t tda18219_read_reg(uint8_t reg);
void tda18219_write_reg(uint8_t reg, uint8_t value);
void tda18219_wait_irq(void);
void tda18219_irq_ack(void);

#endif

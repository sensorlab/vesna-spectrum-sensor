#include <libopencm3/stm32/f1/gpio.h>
#include <libopencm3/stm32/f1/rcc.h>
#include <libopencm3/stm32/f1/rtc.h>
#include <libopencm3/stm32/iwdg.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/systick.h>

#include "cc.h"
#include "vss.h"
#include "timer.h"

#include "device-cc.h"

#if defined(MODEL_SNR_TRX_868) || defined(MODEL_SNR_TRX_2400)

#	define CC_GPIO_NSS	GPIOB
#	define CC_PIN_NSS	GPIO12

#	define CC_GPIO_SPI	GPIOB
#	define CC_PIN_SCK	GPIO13
#	define CC_PIN_MISO	GPIO14
#	define CC_PIN_MOSI	GPIO15

#	define CC_SPI		SPI2

#endif

#if defined(MODEL_SNE_ISMTV_868) || defined(MODEL_SNE_ISMTV_2400)

#	define CC_GPIO_NSS	GPIOB
#	define CC_PIN_NSS	GPIO9

#	define CC_GPIO_SPI	GPIOA
#	define CC_PIN_SCK	GPIO5
#	define CC_PIN_MISO	GPIO6
#	define CC_PIN_MOSI	GPIO7

#	define CC_SPI		SPI1

#endif

int vss_cc_init(void)
{
	u32 br;
	if(CC_SPI == SPI1) {
		rcc_peripheral_enable_clock(&RCC_APB2ENR,
				RCC_APB2ENR_SPI1EN);
		br = SPI_CR1_BAUDRATE_FPCLK_DIV_4;
	} else if(CC_SPI == SPI2) {
		rcc_peripheral_enable_clock(&RCC_APB1ENR,
				RCC_APB1ENR_SPI2EN);
		br = SPI_CR1_BAUDRATE_FPCLK_DIV_2;
	}

	gpio_set_mode(CC_GPIO_NSS, GPIO_MODE_OUTPUT_10_MHZ,
			GPIO_CNF_OUTPUT_PUSHPULL, CC_PIN_NSS);

	gpio_set_mode(CC_GPIO_SPI, GPIO_MODE_OUTPUT_10_MHZ,
			GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, CC_PIN_SCK);
	gpio_set_mode(CC_GPIO_SPI, GPIO_MODE_INPUT,
			GPIO_CNF_INPUT_FLOAT, CC_PIN_MISO);
	gpio_set_mode(CC_GPIO_SPI, GPIO_MODE_OUTPUT_10_MHZ,
			GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, CC_PIN_MOSI);


	spi_init_master(CC_SPI,
			br,
			SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE, 
			SPI_CR1_CPHA_CLK_TRANSITION_1,
			SPI_CR1_DFF_8BIT,
			SPI_CR1_MSBFIRST);
	spi_set_unidirectional_mode(CC_SPI);
	spi_set_full_duplex_mode(CC_SPI);
	spi_enable_software_slave_management(CC_SPI);
	spi_set_nss_high(CC_SPI);

	spi_enable(CC_SPI);

	return vss_cc_reset();
}

static int vss_cc_wait_while_miso_high(void)
{
	while(gpio_get(CC_GPIO_SPI, CC_PIN_MISO));
	return VSS_OK;
}

int vss_cc_reset(void)
{
	gpio_clear(CC_GPIO_NSS, CC_PIN_NSS);
	vss_cc_wait_while_miso_high();

	spi_send(CC_SPI, CC_STROBE_SRES);
	spi_read(CC_SPI);

	vss_cc_wait_while_miso_high();

	gpio_set(CC_GPIO_NSS, CC_PIN_NSS);
	return VSS_OK;
}

int vss_cc_read_reg(uint8_t reg, uint8_t* value)
{
	gpio_clear(CC_GPIO_NSS, CC_PIN_NSS);
	vss_cc_wait_while_miso_high();

	spi_send(CC_SPI, reg|0x80);
	spi_read(CC_SPI);

	spi_send(CC_SPI, 0);
	*value = spi_read(CC_SPI);

	gpio_set(CC_GPIO_NSS,CC_PIN_NSS);

	return VSS_OK;
}

int vss_cc_write_reg(uint8_t reg, uint8_t value)
{
	gpio_clear(CC_GPIO_NSS,CC_PIN_NSS);
	vss_cc_wait_while_miso_high();

	spi_send(CC_SPI, reg);
	spi_read(CC_SPI);

	spi_send(CC_SPI, value);
	spi_read(CC_SPI);

	gpio_set(CC_GPIO_NSS,CC_PIN_NSS);

	return VSS_OK;
}

int vss_cc_strobe(uint8_t strobe) 
{
	gpio_clear(CC_GPIO_NSS, CC_PIN_NSS);
	vss_cc_wait_while_miso_high();

	spi_send(CC_SPI, strobe);
	spi_read(CC_SPI);

	gpio_set(CC_GPIO_NSS, CC_PIN_NSS);

	return VSS_OK;
}

int vss_cc_wait_state(uint8_t state)
{
	uint8_t new_state;
	do {
		vss_cc_read_reg(CC_REG_MARCSTATE, &new_state);
	} while(new_state != state);

	return VSS_OK;
}

void tim4_isr(void)
{
	vss_timer_ack();
	vss_device_cc_timer_isr();
}

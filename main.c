#include <stdio.h>
#include <errno.h>
#include <libopencm3/stm32/f1/rcc.h>
#include <libopencm3/stm32/f1/gpio.h>
#include <libopencm3/stm32/usart.h>

#include "spectrum.h"
#include "dev-null.h"

/* Set up all the peripherals */
void setup(void)
{
	rcc_clock_setup_in_hsi_out_48mhz();

	rcc_peripheral_enable_clock(&RCC_APB2ENR,
			RCC_APB2ENR_IOPAEN |
			RCC_APB2ENR_IOPBEN |
			RCC_APB2ENR_AFIOEN |
			RCC_APB2ENR_USART1EN);

	/* GPIO pin for the LED */
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_2_MHZ,
			GPIO_CNF_OUTPUT_PUSHPULL, GPIO2);

	/* GPIO pin for USART TX */
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,
			GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO9);

	/* Setup USART parameters. */
	usart_set_baudrate(USART1, 115200);
	usart_set_databits(USART1, 8);
	usart_set_stopbits(USART1, USART_STOPBITS_1);
	usart_set_parity(USART1, USART_PARITY_NONE);
	usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);
	usart_set_mode(USART1, USART_MODE_TX);

	/* Finally enable the USART. */
	usart_enable(USART1);
}

/* Provide _write syscall used by libc */
int _write(int file, char *ptr, int len)
{
	int i;

	if (file == 1) {
		for (i = 0; i < len; i++) {
			usart_send_blocking(USART1, ptr[i]);
		}
		return i;
	} else {
		errno = EIO;
		return -1;
	}
}

/* Delay execution for some arbitrary amount of time */
void delay(void)
{
	int i;

	for (i = 0; i < 8000000; i++) {
		__asm__("nop");
	}
}

int main(void)
{
	setup();
	dev_null_register();

	while (1) {

		int n, m;
		for(n = 0; n < spectrum_dev_num; n++) {
			const struct spectrum_dev* dev = spectrum_dev_list[n];

			printf("DEVICE %d: %s\n", n, dev->name);

			for(m = 0; m < dev->dev_config_num; m++) {
				const struct spectrum_dev_config* dev_config = dev->dev_config_list[m];
				printf("  CONFIG %d,%d: %s\n", n, m, dev_config->name);
			}
		}

		delay();
	}

	return 0;
}

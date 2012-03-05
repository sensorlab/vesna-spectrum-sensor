#include <stdio.h>
#include <errno.h>
#include <libopencm3/stm32/f1/rcc.h>
#include <libopencm3/stm32/f1/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/nvic.h>

#include "spectrum.h"
#include "dev-null.h"

#define USART_BUFFER_SIZE		128

char usart_buffer[USART_BUFFER_SIZE];
int usart_buffer_len = 0;
volatile int usart_buffer_attn = 0;

/* Set up all the peripherals */
void setup(void)
{
	rcc_clock_setup_in_hsi_out_48mhz();

	rcc_peripheral_enable_clock(&RCC_APB2ENR,
			RCC_APB2ENR_IOPAEN |
			RCC_APB2ENR_IOPBEN |
			RCC_APB2ENR_AFIOEN |
			RCC_APB2ENR_USART1EN);

	nvic_enable_irq(NVIC_USART1_IRQ);

	/* GPIO pin for the LED */
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_2_MHZ,
			GPIO_CNF_OUTPUT_PUSHPULL, GPIO2);

	/* GPIO pin for USART TX */
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,
			GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO9);

	/* GPIO pin for USART RX */
	gpio_set_mode(GPIOA, GPIO_MODE_INPUT,
		      GPIO_CNF_INPUT_FLOAT, GPIO10);

	/* Setup USART parameters. */
	usart_set_baudrate(USART1, 115200);
	usart_set_databits(USART1, 8);
	usart_set_stopbits(USART1, USART_STOPBITS_1);
	usart_set_parity(USART1, USART_PARITY_NONE);
	usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);
	usart_set_mode(USART1, USART_MODE_TX_RX);

	/* Enable USART1 Receive interrupt. */
	USART_CR1(USART1) |= USART_CR1_RXNEIE;

	/* Finally enable the USART. */
	usart_enable(USART1);
}

void usart1_isr(void)
{
	/* Check if we were called because of RXNE. */
	if (((USART_CR1(USART1) & USART_CR1_RXNEIE) != 0) &&
	    ((USART_SR(USART1) & USART_SR_RXNE) != 0)) {

		/* If we haven't yet processed previous command ignore input */
		if (!usart_buffer_attn) {
			char c = usart_recv(USART1);
			if (c == '\n') {
				usart_buffer[usart_buffer_len] = 0;
				usart_buffer_len = 0;
				usart_buffer_attn = 1;
			} else {
				usart_buffer[usart_buffer_len] = c;
				usart_buffer_len++;
			}
		}
	}
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

void dump_configs(void)
{
	int n, m;
	for(n = 0; n < spectrum_dev_num; n++) {
		const struct spectrum_dev* dev = spectrum_dev_list[n];

		printf("DEVICE %d: %s\n", n, dev->name);

		for(m = 0; m < dev->dev_config_num; m++) {
			const struct spectrum_dev_config* dev_config = dev->dev_config_list[m];
			printf("  CHANNEL CONFIG %d,%d: %s\n", n, m, dev_config->name);
			printf("    BASE: %lld Hz\n", dev_config->channel_base_hz);
			printf("    SPACING: %d Hz\n", dev_config->channel_spacing_hz);
			printf("    BW: %d Hz\n", dev_config->channel_bw_hz);
			printf("    NUM: %d\n", dev_config->channel_num);
			printf("    TIME: %d ms\n", dev_config->channel_time_ms);
		}
	}
}

int main(void)
{
	setup();
	dev_null_register();

	dump_configs();

	printf("\n\n");
	while (1) {
		if (usart_buffer_attn) {
			printf("CMD %s\n", usart_buffer);
			usart_buffer_attn = 0;
		}
	}

	return 0;
}

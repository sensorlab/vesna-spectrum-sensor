/* Copyright (C) 2012 SensorLab, Jozef Stefan Institute
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
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <libopencm3/stm32/f1/rcc.h>
#include <libopencm3/stm32/f1/gpio.h>
#include <libopencm3/stm32/f1/scb.h>
#include <libopencm3/stm32/iwdg.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/nvic.h>

#include "spectrum.h"
#include "dev-dummy.h"
#include "dev-tda18219.h"
#include "dev-cc.h"

#include "run.h"
#include "device-dummy.h"

#define USART_BUFFER_SIZE		128
#define DATA_BUFFER_SIZE		32

static char usart_buffer[USART_BUFFER_SIZE];
static int usart_buffer_len = 0;
static volatile int usart_buffer_attn = 0;

static struct vss_sweep_config current_sweep_config;

static struct vss_device_run current_device_run;
static uint16_t data_buffer[DATA_BUFFER_SIZE];

extern void (*const vector_table[]) (void);

/* Set up all the peripherals */

static void setup_usart(void) 
{
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

static void setup(void)
{
	SCB_VTOR = (u32) vector_table;

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

	setup_usart();
}

void usart1_isr(void)
{
	/* Check if we were called because of RXNE. */
	if (((USART_CR1(USART1) & USART_CR1_RXNEIE) != 0) &&
	    ((USART_SR(USART1) & USART_SR_RXNE) != 0)) {

		char c = usart_recv(USART1);

		/* If we haven't yet processed previous command ignore input */
		if (!usart_buffer_attn) {
			if (c == '\n' || usart_buffer_len >= (USART_BUFFER_SIZE-1)) {
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

static void command_help(void)
{
	printf( "VESNA spectrum sensing application\n\n"

		"help         print this help message\n"
		"list         list available devices and pre-set configuations\n"
		"report-on    start spectrum sweep\n"
		"report-off   stop spectrum sweep\n"
		"select channel START:STEP:STOP config DEVICE,CONFIG\n"
		"             sweep channels from START to STOP stepping STEP\n"
		"             channels at a time using DEVICE and CONFIG pre-set\n"
		"status       print out hardware status\n\n"

		"sweep data has the following format:\n"
		"             TS timestamp DS power ... DE\n"
		"where timestamp is time in seconds since sweep start and power is\n"
		"received signal power for corresponding channel in dBm\n");
}

static void command_list(void)
{
	int dev_id = -1;
	int config_id = 0;

	const struct vss_device* device = NULL;

	int n;
	for(n = 0; n < vss_device_config_list_num; n++) {
		const struct vss_device_config* device_config = vss_device_config_list[n];

		if(device_config->device != device) {
			device = device_config->device;
			dev_id++;

			printf("device %d: %s\n", dev_id, device->name);

			config_id = 0;
		}

		printf("  channel config %d,%d: %s\n", dev_id, config_id, device_config->name);
		printf("    base: %lld Hz\n", device_config->channel_base_hz);
		printf("    spacing: %d Hz\n", device_config->channel_spacing_hz);
		printf("    bw: %d Hz\n", device_config->channel_bw_hz);
		printf("    num: %d\n", device_config->channel_num);
		printf("    time: %d ms\n", device_config->channel_time_ms);

		config_id++;
	}
}

static void command_report_on(void)
{
	if (current_sweep_config.device_config == NULL) {
		printf("error: set channel config first\n");
	} else {
		vss_device_run_init(&current_device_run, &current_sweep_config, -1, data_buffer);
		vss_device_run_start(&current_device_run);
	}
}

static void command_report_off(void)
{
	vss_device_run_stop(&current_device_run);
}

static void command_select(int start, int step, int stop, int dev_id, int config_id) 
{
	const struct vss_device_config* device_config = vss_device_config_get(dev_id, config_id);

	if(device_config == NULL) {
		printf("error: unknown config %d,%d\n", dev_id, config_id);
		return;
	}

	current_sweep_config.device_config = device_config;
	current_sweep_config.channel_start = start;
	current_sweep_config.channel_step = step;
	current_sweep_config.channel_stop = stop;

	printf("ok\n");
}

static void command_version(void)
{
	printf("%s\n", VERSION);
}

static void command_status(void)
{
#ifdef TUNER_TDA18219
	dev_tda18219_print_status();
#endif

#ifdef TUNER_CC
#if defined(MODEL_SNR_TRX_868) || defined(MODEL_SNE_ISMTV_868)
	dev_cc1101_print_status();
#endif

#if defined(MODEL_SNR_TRX_2400) || defined(MODEL_SNE_ISMTV_2400)
	dev_cc2500_print_status();
#endif
#endif
}

static void dispatch(const char* cmd)
{
	int start, stop, step, dev_id, config_id;

	if (!strcmp(cmd, "help")) {
		command_help();
	} else if (!strcmp(cmd, "list")) {
		command_list();
	} else if (!strcmp(cmd, "report-on")) {
		command_report_on();
	} else if (!strcmp(cmd, "report-off")) {
		command_report_off();
	} else if (!strcmp(cmd, "status")) {
		command_status();
	} else if (sscanf(cmd, "select channel %d:%d:%d config %d,%d", 
				&start, &step, &stop,
				&dev_id, &config_id) == 5) {
		command_select(start, step, stop, dev_id, config_id);
	} else if (!strcmp(cmd, "version")) {
		command_version();
	} else {
		printf("error: unknown command: %s\n", cmd);
	}
}

int main(void)
{
	setup();

#ifdef TUNER_TDA18219
	dev_tda18219_register();
#endif

#ifdef TUNER_CC
	dev_cc_register();
#endif

#ifdef TUNER_NULL
	vss_device_dummy_register();
#endif

	unsigned int last_overflow_num = 0;

	while(1) {
		if (usart_buffer_attn) {
			dispatch(usart_buffer);
			usart_buffer_attn = 0;
		}

		IWDG_KR = IWDG_KR_RESET;

		if(current_device_run.overflow_num != last_overflow_num) {
			last_overflow_num = current_device_run.overflow_num;
			printf("error: overflow\n");
		}

		struct vss_device_run_read_result ctx;
		vss_device_run_read(&current_device_run, &ctx);

		int channel;
		uint32_t timestamp;
		uint16_t power;

		while(vss_device_run_read_parse(&current_device_run, &ctx,
								&timestamp, &channel, &power) == VSS_OK) {
			if(channel != -1) {
				if(channel == 0) {
					printf("TS %ld.%03ld DS", timestamp/1000, timestamp%1000);
				}

				printf(" %d.%02d", power/100, abs(power%100));

				if(channel + current_device_run.sweep_config->channel_step
						>= current_device_run.sweep_config->channel_stop) {
					printf(" DE\n");
				}
			}
		}
	}

	return 0;
}

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
#include <libopencm3/stm32/f1/rtc.h>
#include <libopencm3/stm32/f1/scb.h>
#include <libopencm3/stm32/iwdg.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/nvic.h>

#include "spectrum.h"
#include "dev-dummy.h"
#include "dev-tda18219.h"
#include "dev-cc.h"

#define USART_BUFFER_SIZE		128

static char usart_buffer[USART_BUFFER_SIZE];
static int usart_buffer_len = 0;
static volatile int usart_buffer_attn = 0;
static int report = 0;

static struct spectrum_sweep_config sweep_config;
static const struct spectrum_dev* dev = NULL;

extern void (*const vector_table[]) (void);

/* Set up all the peripherals */

static void setup_rtc(void) 
{
	rtc_awake_from_off(LSE);
	rtc_set_prescale_val(15);
}

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
	setup_rtc();
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

static int report_cb(const struct spectrum_sweep_config* sweep_config, int timestamp, const short int data_list[])
{
	int channel_num = spectrum_sweep_channel_num(sweep_config);
	int n;
	printf("TS %d.%03d DS", timestamp/1000, timestamp%1000);
	for(n = 0; n < channel_num; n++) {
		printf(" %d.%02d", data_list[n]/100, abs(data_list[n]%100));
	}
	printf(" DE\n");

	if(usart_buffer_attn) {
		return E_SPECTRUM_STOP_SWEEP;
	} else {
		return E_SPECTRUM_OK;
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
	int dev_id, config_id;
	for(dev_id = 0; dev_id < spectrum_dev_num; dev_id++) {
		const struct spectrum_dev* dev = spectrum_dev_list[dev_id];

		printf("device %d: %s\n", dev_id, dev->name);

		for(config_id = 0; config_id < dev->dev_config_num; config_id++) {
			const struct spectrum_dev_config* dev_config = dev->dev_config_list[config_id];
			printf("  channel config %d,%d: %s\n", dev_id, config_id, dev_config->name);
			printf("    base: %lld Hz\n", dev_config->channel_base_hz);
			printf("    spacing: %d Hz\n", dev_config->channel_spacing_hz);
			printf("    bw: %d Hz\n", dev_config->channel_bw_hz);
			printf("    num: %d\n", dev_config->channel_num);
			printf("    time: %d ms\n", dev_config->channel_time_ms);
		}
	}
}

static void command_report_on(void)
{
	if (dev == NULL) {
		printf("error: set channel config first\n");
	} else {
		report = 1;
	}
}

static void command_report_off(void)
{
	report = 0;
	printf("ok\n");
}

static void command_select(int start, int step, int stop, int dev_id, int config_id) 
{
	if (dev_id < 0 || dev_id >= spectrum_dev_num) {
		printf("error: unknown device %d\n", dev_id);
		return;
	}

	dev = spectrum_dev_list[dev_id];

	if (config_id < 0 || config_id >= dev->dev_config_num) {
		printf("error: unknown config %d\n", config_id);
	}

	const struct spectrum_dev_config* dev_config = dev->dev_config_list[config_id];

	sweep_config.dev_config = dev_config;
	sweep_config.channel_start = start;
	sweep_config.channel_step = step;
	sweep_config.channel_stop = stop;

	sweep_config.cb = report_cb;

	printf("ok\n");
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
	dev_dummy_register();
#endif

	int r;
	
	r = spectrum_reset();
	if (r) {
		printf("spectrum_reset(): error %d\n", r);
	}

	while (1) {
		if (usart_buffer_attn) {
			dispatch(usart_buffer);
			usart_buffer_attn = 0;
		}
		IWDG_KR = IWDG_KR_RESET;
		if (report) {
			r = spectrum_run(dev, &sweep_config);
			if (r) {
				printf("error: spectrum_run(): %d\n", r);
			}
		}
	}

	return 0;
}

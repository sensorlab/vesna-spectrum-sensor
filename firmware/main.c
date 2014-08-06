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
#include "config.h"

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

#include "device-dummy.h"
#include "device-cc.h"
#include "device-tda18219.h"

#include "task.h"
#include "version.h"
#include "rcc.h"

#define BASEBAND_SAMPLE_NUM		1024
#define USART_BUFFER_SIZE		128
#define DATA_BUFFER_SIZE		32

static char usart_buffer[USART_BUFFER_SIZE];
static int usart_buffer_len = 0;
static volatile int usart_buffer_attn = 0;

static struct vss_sweep_config current_sweep_config = {
	.device_config = NULL,
#ifdef TUNER_TDA18219
	.n_average = 100
#else
	.n_average = 10
#endif
};

static struct vss_task current_task;
static power_t data_buffer[DATA_BUFFER_SIZE];
static int has_started = 0;

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

	rcc_clock_setup_in_hsi_out_56mhz();

	rcc_peripheral_enable_clock(&RCC_APB2ENR,
			RCC_APB2ENR_IOPAEN |
			RCC_APB2ENR_IOPBEN |
			RCC_APB2ENR_AFIOEN |
			RCC_APB2ENR_USART1EN);

	nvic_enable_irq(NVIC_USART1_IRQ);
	nvic_set_priority(NVIC_USART1_IRQ, 0);

	/* GPIO pin for the LED */
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_2_MHZ,
			GPIO_CNF_OUTPUT_PUSHPULL, GPIO2);

	setup_usart();
}

static void led_on(void)
{
	gpio_set(GPIOB, GPIO2);
}

static void led_off(void)
{
	gpio_clear(GPIOB, GPIO2);
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
		led_on();
		for (i = 0; i < len; i++) {
			usart_send_blocking(USART1, ptr[i]);
		}
		led_off();
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
		"baseband     obtain a continuous string of baseband samples\n"
		"average N    set number of hardware samples to average for one\n"
		"             datapoint\n"
		"status       print out hardware status\n"
		"version      print out firmware version\n\n"

		"calib-off    turn off calibration\n\n"

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
	} else if (has_started) {
		printf("error: stop current sweep first\n");
	} else {
		vss_task_init(&current_task, &current_sweep_config, -1, data_buffer);

		int r = vss_task_start(&current_task);
		if(r) {
			printf("error: vss_task_start returned %d\n", r);
		}

		has_started = 1;
	}
}

static void command_report_off(void)
{
	if(!has_started) {
		printf("ok\n");
	} else {
		vss_task_stop(&current_task);
	}
}

static void command_baseband(void)
{
	if(current_sweep_config.device_config == NULL) {
		printf("error: set channel config first\n");
	} else if(has_started) {
		printf("error: stop current sweep first\n");
	} else {
		power_t buffer[BASEBAND_SAMPLE_NUM];
		int r = vss_device_baseband(current_sweep_config.device_config->device,
				&current_sweep_config, buffer, BASEBAND_SAMPLE_NUM);
		if(r == VSS_NOT_SUPPORTED) {
			printf("error: device doesn't support baseband sampling\n");
		} else if(r) {
			printf("error: vss_device_baseband returned %d\n", r);
		} else {
			int n;
			printf("DS");
			for(n = 0; n < BASEBAND_SAMPLE_NUM; n++) {
				printf(" %hd", buffer[n]);
			}
			printf(" DE\n");
			printf("ok\n");
		}
	}
}

static void command_select(int start, int step, int stop, int dev_id, int config_id) 
{
	if(has_started) {
		printf("error: stop current sweep first\n");
		return;
	}

	const struct vss_device_config* device_config = vss_device_config_get(dev_id, config_id);

	if(device_config == NULL) {
		printf("error: unknown config %d,%d\n", dev_id, config_id);
		return;
	}

	current_sweep_config.device_config = device_config;
	current_sweep_config.channel_start = start;
	current_sweep_config.channel_step = step;
	current_sweep_config.channel_stop = stop;

	const struct calibration_point* calibration_data =
		vss_device_get_calibration(device_config->device, device_config);

	if(calibration_data != NULL) {
		calibration_set_data(calibration_data);
	} else {
		calibration_set_data(calibration_empty_data);
	}

	printf("ok\n");
}

static void command_average(int n_average)
{
	if(has_started) {
		printf("error: stop current sweep first\n");
		return;
	}

	current_sweep_config.n_average = n_average;

	printf("ok\n");
}

static void command_version(void)
{
	printf("%s\n", VERSION);
}

static void command_status(void)
{
	if (current_sweep_config.device_config == NULL) {
		printf("error: set channel config first\n");
	} else {
		char buff[1024];
		int r = vss_device_status(current_sweep_config.device_config->device, buff, sizeof(buff));
		if(r != VSS_OK) {
			printf("error: vss_device_status returned %d\n", r);
		} else {
			printf("%s", buff);
		}
	}
}

static void command_calib_off(void)
{
	calibration_set_data(calibration_empty_data);
	printf("ok\n");
}

static void dispatch(const char* cmd)
{
	int start, stop, step, dev_id, config_id;
	int n_average;

	if (!strcmp(cmd, "help")) {
		command_help();
	} else if (!strcmp(cmd, "list")) {
		command_list();
	} else if (!strcmp(cmd, "report-on")) {
		command_report_on();
	} else if (!strcmp(cmd, "report-off")) {
		command_report_off();
	} else if (!strcmp(cmd, "baseband")) {
		command_baseband();
	} else if (!strcmp(cmd, "status")) {
		command_status();
	} else if (sscanf(cmd, "select channel %d:%d:%d config %d,%d", 
				&start, &step, &stop,
				&dev_id, &config_id) == 5) {
		command_select(start, step, stop, dev_id, config_id);
	} else if (sscanf(cmd, "average %d",
				&n_average) == 1) {
		command_average(n_average);
	} else if (!strcmp(cmd, "version")) {
		command_version();
	} else if (!strcmp(cmd, "calib-off")) {
		command_calib_off();
	} else {
		printf("error: unknown command: %s\n", cmd);
	}
}

int main(void)
{
	setup();
	printf("boot\n");

	int r = VSS_OK;
#ifdef TUNER_TDA18219
	r = vss_device_tda18219_register();
#endif

#ifdef TUNER_CC
	r = vss_device_cc_register();
#endif

#ifdef TUNER_NULL
	r = vss_device_dummy_register();
#endif
	if(r != VSS_OK) {
		printf("error: registering device: %d\n", r);
	}

	while(1) {
		if (usart_buffer_attn) {
			dispatch(usart_buffer);
			usart_buffer_attn = 0;
		}

		IWDG_KR = IWDG_KR_RESET;

		int has_finished = (vss_task_get_state(&current_task) == VSS_DEVICE_RUN_FINISHED);

		struct vss_task_read_result ctx;
		vss_task_read(&current_task, &ctx);

		int channel;
		uint32_t timestamp;
		power_t power;

		while(vss_task_read_parse(&current_task, &ctx,
								&timestamp, &channel, &power) == VSS_OK) {
			if(channel >= 0) {
				if((unsigned) channel == current_task.sweep_config->channel_start) {
					printf("TS %ld.%03ld DS", timestamp/1000, timestamp%1000);
				}

				printf(" %d.%02d", power/100, abs(power%100));

				if(channel + current_task.sweep_config->channel_step
						>= current_task.sweep_config->channel_stop) {
					printf(" DE\n");
				}

			}
		}

		if(has_finished && has_started) {
			const char* msg = vss_task_get_error(&current_task);
			if(msg) {
				printf("error: %s\n", msg);
			} else {
				printf("ok\n");
			}
			has_started = 0;
		}
	}

	return 0;
}

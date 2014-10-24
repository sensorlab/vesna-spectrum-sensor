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
#include <ctype.h>
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
#include "rcc.h"
#include "version.h"

#define USART_BUFFER_SIZE		128
#define DATA_BUFFER_SIZE		26000

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

static void setup_usart(uint32_t usart)
{
	uint32_t baudrate;

	if(usart == USART1) {
		/* GPIO pin for USART TX */
		gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,
				GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO9);

		/* GPIO pin for USART RX */
		gpio_set_mode(GPIOA, GPIO_MODE_INPUT,
			      GPIO_CNF_INPUT_FLOAT, GPIO10);

		baudrate = 576000;
	} else {
		// GPIO_PinRemapConfig(GPIO_PartialRemap_USART3, ENABLE);
		uint32_t r = AFIO_MAPR;
		r &= ~AFIO_MAPR_USART3_REMAP_FULL_REMAP;
		r |= AFIO_MAPR_USART3_REMAP_PARTIAL_REMAP;
		AFIO_MAPR = r;

		/* GPIO pin for USART TX */
		gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_50_MHZ,
				GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO10);

		/* GPIO pin for USART RX */
		gpio_set_mode(GPIOC, GPIO_MODE_INPUT,
			      GPIO_CNF_INPUT_FLOAT, GPIO11);

		// maximum supported by DigiConnect ME
		baudrate = 230400;
	}

	/* Setup USART parameters. */
	usart_set_baudrate(usart, baudrate);
	usart_set_databits(usart, 8);
	usart_set_stopbits(usart, USART_STOPBITS_1);
	usart_set_parity(usart, USART_PARITY_NONE);
	usart_set_flow_control(usart, USART_FLOWCONTROL_NONE);
	usart_set_mode(usart, USART_MODE_TX_RX);

	/* Enable usart Receive interrupt. */
	USART_CR1(usart) |= USART_CR1_RXNEIE;

	/* Finally enable the USART. */
	usart_enable(usart);
}

static void setup(void)
{
	SCB_VTOR = (u32) vector_table;
	asm volatile ("cpsie i");

	rcc_clock_setup_in_hsi_out_56mhz();

	rcc_peripheral_enable_clock(&RCC_APB2ENR,
			RCC_APB2ENR_IOPAEN |
			RCC_APB2ENR_IOPBEN |
			RCC_APB2ENR_IOPCEN |
			RCC_APB2ENR_AFIOEN);

	uint8_t irqn;
	rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_USART1EN);

	if(VSS_UART == USART1) {
		irqn = NVIC_USART1_IRQ;
	} else {
		rcc_peripheral_enable_clock(&RCC_APB1ENR,
			RCC_APB1ENR_USART3EN);
		irqn = NVIC_USART3_IRQ;
	}

	nvic_enable_irq(irqn);
	nvic_set_priority(irqn, 0);

	/* GPIO pin for the LED */
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_2_MHZ,
			GPIO_CNF_OUTPUT_PUSHPULL, GPIO2);

	setup_usart(VSS_UART);
	if(VSS_UART != USART1) {
		setup_usart(USART1);
	}

	/* Setup watchdog */
	iwdg_set_period_ms(10000);
	iwdg_start();
}

static void led_on(void)
{
	gpio_set(GPIOB, GPIO2);
}

static void led_off(void)
{
	gpio_clear(GPIOB, GPIO2);
}

void usart_isr(void)
{
	/* Check if we were called because of RXNE. */
	if (((USART_CR1(VSS_UART) & USART_CR1_RXNEIE) != 0) &&
	    ((USART_SR(VSS_UART) & USART_SR_RXNE) != 0)) {

		char c = usart_recv(VSS_UART);

		/* If we haven't yet processed previous command ignore input */
		if (!usart_buffer_attn) {
			if (c == '\n' || c == 0 || usart_buffer_len >= (USART_BUFFER_SIZE-1)) {
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

void usart1_isr(void)
{
	usart_isr();
}

void usart3_isr(void)
{
	usart_isr();
}

static int write(char *ptr, int len, uint32_t usart)
{
	int i;
	for(i = 0; i < len; i++) {
		usart_send_blocking(usart, ptr[i]);
	}
	return i;
}

/* Provide _write syscall used by libc */
int _write(int file, char *ptr, int len)
{
	if(file == 1) {
		led_on();
		int r = write(ptr, len, VSS_UART);
		led_off();
		return r;
	} else {
		errno = EIO;
		return -1;
	}
}

static void debug(char *ptr)
{
	if(VSS_UART != USART1) {
		write(ptr, strlen(ptr), USART1);
	}
}

static void command_help(void)
{
	printf( "VESNA spectrum sensing application\n\n"

		"help         print this help message\n"
		"version      print out firmware version\n\n"

		"status       print out hardware status\n"
		"list         list available devices and pre-set configuations\n\n"

		"samples N    set number of samples\n"
		"select channel CH config DEVICE,CONFIG\n"
		"             sample channel CH using DEVICE and CONFIG pre-set\n"
		"sample-on    start sampling channel\n"
		"sample-off   stop sampling channel\n\n"

		"select channel START:STEP:STOP config DEVICE,CONFIG\n"
		"             sweep channels from START to STOP stepping STEP\n"
		"             channels at a time using DEVICE and CONFIG pre-set\n"
		"report-on    start spectrum sweep\n"
		"report-off   stop spectrum sweep\n"
		"average N    set number of hardware samples to average for one\n"
		"             datapoint\n\n"

		"calib-off    turn off calibration\n\n"

		"sweep data has the following format:\n"
		"             TS timestamp DS power ... DE\n"
		"where timestamp is time in seconds since sweep start and power is\n"
		"received signal power for corresponding channel in dBm\n\n"

		"sample data has the following format:\n"
		"             TS timestamp DS sample ... DE\n"
		"where timestamp is time in seconds since sweep start and sample is\n"
		"signal sample in DAC digits\n");
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
			if(device->supports_task_baseband) {
				printf("  device supports channel sampling\n");
			}

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

static void command_task_on(enum vss_task_type type)
{
	if (current_sweep_config.device_config == NULL) {
		printf("error: set channel config first\n");
	} else if (has_started) {
		printf("error: stop current sweep first\n");
	} else {
		int r;
		r = vss_task_init(&current_task, type,
				&current_sweep_config, -1, data_buffer);
		if(r) {
			if(r == VSS_TOO_MANY) {
				printf("error: not enough memory for task\n");
			} else {
				printf("error: vss_task_init returned %d\n", r);
			}
			return;
		}

		r = vss_task_start(&current_task);
		if(r) {
			printf("error: vss_task_start returned %d\n", r);
		}

		has_started = 1;
	}
}

static void command_sweep_on(void)
{
	command_task_on(VSS_TASK_SWEEP);
}

static void command_sample_on(void)
{
	command_task_on(VSS_TASK_SAMPLE);
}

static void command_task_off(void)
{
	if(!has_started) {
		printf("ok\n");
	} else {
		vss_task_stop(&current_task);
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

static char* trim(char* cmd)
{
	char* end = cmd + strlen(cmd) - 1;
	while(end > cmd && isspace((int) *end)) end--;
	end++;

	*end = 0;

	return cmd;
}

static void dispatch(char* cmdi)
{
	int start, stop, step, dev_id, config_id;
	int n_average;

	char* cmd = trim(cmdi);

	debug("dispatch: received command '"); debug(cmd); debug("'\n");

	if (!strcmp(cmd, "help")) {
		command_help();
	} else if (!strcmp(cmd, "list")) {
		command_list();
	} else if (!strcmp(cmd, "sweep-on") || !strcmp(cmd, "report-on")) {
		command_sweep_on();
	} else if (!strcmp(cmd, "sample-on")) {
		command_sample_on();
	} else if (!strcmp(cmd, "sweep-off") || !strcmp(cmd, "sample-off") ||
						!strcmp(cmd, "report-off")) {
		command_task_off();
	} else if (!strcmp(cmd, "status")) {
		command_status();
	} else if (sscanf(cmd, "select channel %d:%d:%d config %d,%d", 
				&start, &step, &stop,
				&dev_id, &config_id) == 5) {
		command_select(start, step, stop, dev_id, config_id);
	} else if (sscanf(cmd, "select channel %d config %d,%d",
				&start,
				&dev_id, &config_id) == 3) {
		command_select(start, 1, start+1, dev_id, config_id);
	} else if (sscanf(cmd, "average %d",
				&n_average) == 1) {
		command_average(n_average);
	} else if (sscanf(cmd, "samples %d",
				&n_average) == 1) {
		command_average(n_average);
	} else if (!strcmp(cmd, "version")) {
		command_version();
	} else if (!strcmp(cmd, "calib-off")) {
		command_calib_off();
	} else if (!strcmp(cmd, "\x02\xff")) {
		// "\x02\xff\x00" is a XCP connect command. It means
		// someone wants to talk to the bootloader. Hence we
		// reset the system to start the bootloader...
		scb_reset_system();
	} else {
		printf("error: unknown command: %s\n", cmd);
		debug("dispatch: unknown command\n");
	}
}

int main(void)
{
	setup();
	printf("boot\n");
	debug("boot\n");

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

		iwdg_reset();

		int has_finished = (vss_task_get_state(&current_task) == VSS_DEVICE_RUN_FINISHED);

		int n = 0;

		struct vss_task_read_result ctx;
		if(vss_task_read(&current_task, &ctx) == VSS_OK) {

			unsigned int channel = ctx.block->channel;
			uint32_t timestamp = ctx.block->timestamp;
			power_t power;

			printf("TS %ld.%03ld CH %u DS", timestamp/1000,
					timestamp%1000, channel);

			while(vss_task_read_parse(&ctx, &timestamp, &channel, &power) == VSS_OK) {

				if(current_task.type == VSS_TASK_SWEEP) {
					printf(" %d.%02d", power/100, abs(power%100));
				} else {
					printf(" %d", power);
				}
			}

			printf(" DE\n");
			debug("main: wrote block report\n");
			n = 1;
		}

		if(n == 0 && has_finished && has_started) {
			const char* msg = vss_task_get_error(&current_task);
			if(msg) {
				printf("error: %s\n", msg);
			} else {
				printf("ok\n");
				if(current_task.overflows > 0) {
					printf("%d overflows\n",
							current_task.overflows);
				}
			}
			has_started = 0;
			debug("main: task finished\n");
		}
	}

	return 0;
}

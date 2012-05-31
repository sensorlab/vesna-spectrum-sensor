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

/* Authors:	Ales Verbic
 * 		Zoltan Padrah
 * 		Tomaz Solc, <tomaz.solc@ijs.si> */
#include <stdio.h>
#include <stdlib.h>
#include <libopencm3/stm32/f1/gpio.h>
#include <libopencm3/stm32/f1/rcc.h>
#include <libopencm3/stm32/f1/rtc.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/systick.h>

#include "dev-cc.h"
#include "spectrum.h"

static void setup_stm32f1_peripherals(void)
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


	systick_set_reload(0x00ffffff);
	systick_set_clocksource(STK_CTRL_CLKSOURCE_AHB_DIV8);
	systick_counter_enable();
}

static void cc_wait_while_miso_high(void)
{
	while(gpio_get(CC_GPIO_SPI, CC_PIN_MISO));
}

static const uint32_t systick_udelay_calibration = 6;

static void systick_udelay(uint32_t usecs)
{
	uint32_t val = (STK_VAL - systick_udelay_calibration * usecs) 
		& 0x00ffffff;
	while(!((STK_VAL - val) & 0x00800000));
}

static void cc_reset() 
{
	gpio_clear(CC_GPIO_NSS, CC_PIN_NSS);
	cc_wait_while_miso_high();

	spi_send(CC_SPI, CC_STROBE_SRES);
	cc_wait_while_miso_high();

	gpio_set(CC_GPIO_NSS, CC_PIN_NSS);
}

uint16_t cc_read_reg(uint8_t reg)
{
	gpio_clear(CC_GPIO_NSS, CC_PIN_NSS);
	cc_wait_while_miso_high();

	spi_send(CC_SPI, reg|0x80);
	spi_read(CC_SPI);

	spi_send(CC_SPI, 0);
	uint16_t value = spi_read(CC_SPI);

	gpio_set(CC_GPIO_NSS,CC_PIN_NSS);

	return value;
}

void cc_write_reg(uint8_t reg, uint8_t value) 
{
	gpio_clear(CC_GPIO_NSS,CC_PIN_NSS);
	cc_wait_while_miso_high();

	spi_send(CC_SPI, reg);
	spi_read(CC_SPI);

	spi_send(CC_SPI, value);
	spi_read(CC_SPI);

	gpio_set(CC_GPIO_NSS,CC_PIN_NSS);
}

uint16_t cc_strobe(uint8_t strobe) 
{
	gpio_clear(CC_GPIO_NSS, CC_PIN_NSS);
	cc_wait_while_miso_high();

	spi_send(CC_SPI, strobe);
	uint16_t value = spi_read(CC_SPI);

	gpio_set(CC_GPIO_NSS, CC_PIN_NSS);

	return value;
}

void cc_wait_state(uint8_t state)
{
	while(cc_read_reg(CC_REG_MARCSTATE) != state);
}

int dev_cc_reset(void* priv) 
{
	setup_stm32f1_peripherals();
	cc_reset();
	return E_SPECTRUM_OK;
}

int dev_cc_setup(void* priv, const struct spectrum_sweep_config* sweep_config) 
{
	uint8_t *init_seq = (uint8_t*) sweep_config->dev_config->priv;

	cc_strobe(CC_STROBE_SIDLE);
	cc_wait_state(CC_MARCSTATE_IDLE);

	int n;
	for(n = 0; init_seq[n] != 0xff; n += 2) {
		uint8_t reg = init_seq[n];
		uint8_t value = init_seq[n+1];
		cc_write_reg(reg, value);
	}

	return E_SPECTRUM_OK;
}

int dev_cc_run(void* priv, const struct spectrum_sweep_config* sweep_config)
{
	int r;
	short int *data;
	/* FIXME: calculate value according to the formula in the datasheet */
	const uint32_t rssi_delay_us = 5000;

	rtc_set_counter_val(0);

	int channel_num = spectrum_sweep_channel_num(sweep_config);
	data = calloc(channel_num, sizeof(*data));
	if (data == NULL) {
		return E_SPECTRUM_TOOMANY;
	}

	do {
		uint32_t rtc_counter = rtc_get_counter_val();
		/* LSE clock is 32768 Hz. Prescaler is set to 16.
		 *
		 *                 rtc_counter * 16
		 * t [ms] = 1000 * ----------------
		 *                       32768
		 */
		int timestamp = ((long long) rtc_counter) * 1000 / 2048;

		int n, ch;
		for(		ch = sweep_config->channel_start, n = 0; 
				ch < sweep_config->channel_stop && n < channel_num; 
				ch += sweep_config->channel_step, n++) {

			cc_strobe(CC_STROBE_SIDLE);
			cc_wait_state(CC_MARCSTATE_IDLE);

			cc_write_reg(CC_REG_CHANNR, ch);

			cc_strobe(CC_STROBE_SRX);
			cc_wait_state(CC_MARCSTATE_RX);

			systick_udelay(rssi_delay_us);

			int8_t reg = cc_read_reg(CC_REG_RSSI);

			int rssi_dbm_100 = -5920 + ((int) reg) * 50;
			data[n] = rssi_dbm_100;
		}
		r = sweep_config->cb(sweep_config, timestamp, data);
	} while(!r);

	free(data);

	if (r == E_SPECTRUM_STOP_SWEEP) {
		return E_SPECTRUM_OK;
	} else {
		return r;
	}
}

static void dev_cc_print_status(void)
{
	printf("Part number : %02x\n", cc_read_reg(CC_REG_PARTNUM));
	printf("Version     : %02x\n", cc_read_reg(CC_REG_VERSION));
}

void dev_cc1101_print_status(void)
{
	printf("IC          : CC1101\n\n");
	dev_cc_print_status();
}

void dev_cc2500_print_status(void)
{
	printf("IC          : CC2500\n\n");
	dev_cc_print_status();
}

uint8_t dev_cc1101_868mhz_60khz_init_seq[] = {
	/* Channel spacing = 49.953461
	 * RX filter BW = 60.267857
	 * Base frequency = 862.999695 */
	CC_REG_IOCFG2,        0x29,
	CC_REG_IOCFG1,        0x2E,
	CC_REG_IOCFG0,        0x06,
	CC_REG_FIFOTHR,       0x47,
	CC_REG_SYNC1,         0xD3,
	CC_REG_SYNC0,         0x91,
	CC_REG_PKTLEN,        0xFF,
	CC_REG_PKTCTRL1,      0x04,
	CC_REG_PKTCTRL0,      0x05,
	CC_REG_ADDR,          0x00,
	CC_REG_CHANNR,        0x00,
	CC_REG_FSCTRL1,       0x06,
	CC_REG_FSCTRL0,       0x00,
	CC_REG_FREQ2,         0x1F,
	CC_REG_FREQ1,         0xF6,
	CC_REG_FREQ0,         0x84,
	CC_REG_MDMCFG4,       0xF5,
	CC_REG_MDMCFG3,       0x75,
	CC_REG_MDMCFG2,       0x13,
	CC_REG_MDMCFG1,       0x20,
	CC_REG_MDMCFG0,       0xE5,
	CC_REG_DEVIATN,       0x67,
	CC_REG_MCSM2,         0x07,
	CC_REG_MCSM1,         0x30,
	CC_REG_MCSM0,         0x18,
	CC_REG_FOCCFG,        0x16,
	CC_REG_BSCFG,         0x6C,
	CC_REG_AGCCTRL2,      0x03,
	CC_REG_AGCCTRL1,      0x40,
	CC_REG_AGCCTRL0,      0x91,
	CC_REG_WOREVT1,       0x87,
	CC_REG_WOREVT0,       0x6B,
	CC_REG_WORCTRL,       0xFB,
	CC_REG_FREND1,        0x56,
	CC_REG_FREND0,        0x10,
	CC_REG_FSCAL3,        0xE9,
	CC_REG_FSCAL2,        0x2A,
	CC_REG_FSCAL1,        0x00,
	CC_REG_FSCAL0,        0x1F,
	CC_REG_RCCTRL1,       0x41,
	CC_REG_RCCTRL0,       0x00,
	CC_REG_FSTEST,        0x59,
	CC_REG_PTEST,         0x7F,
	CC_REG_AGCTEST,       0x3F,
	CC_REG_TEST2,         0x81,
	CC_REG_TEST1,         0x35,
	CC_REG_TEST0,         0x09,
	CC_REG_PARTNUM,       0x00,
	CC_REG_VERSION,       0x04,
	CC_REG_FREQEST,       0x00,
	CC_REG_LQI,           0x00,
	CC_REG_RSSI,          0x00,
	CC_REG_MARCSTATE,     0x00,
	CC_REG_WORTIME1,      0x00,
	CC_REG_WORTIME0,      0x00,
	CC_REG_PKTSTATUS,     0x00,
	CC_REG_VCO_VC_DAC,    0x00,
	CC_REG_TXBYTES,       0x00,
	CC_REG_RXBYTES,       0x00,
	CC_REG_RCCTRL1_STATUS,0x00,
	CC_REG_RCCTRL0_STATUS,0x00,
	0xFF,                 0xFF
};

uint8_t dev_cc1101_868mhz_100khz_init_seq[] = {
	CC_REG_IOCFG2,             0x2E,
	CC_REG_IOCFG1,             0x2E,
	CC_REG_IOCFG0,             0x2E,
	CC_REG_FIFOTHR,            0x07,
	CC_REG_SYNC1,              0xD3,
	CC_REG_SYNC0,              0x91,
	CC_REG_PKTLEN,             0xFF,
	CC_REG_PKTCTRL1,           0x04,
	CC_REG_PKTCTRL0,           0x12,
	CC_REG_ADDR,               0x00,
	CC_REG_CHANNR,             0x00,
	CC_REG_FSCTRL1,            0x06,
	CC_REG_FSCTRL0,            0x00,
	/* fc = 835.919678 MHz */
	CC_REG_FREQ2,              0x20,
	CC_REG_FREQ1,              0x26,
	CC_REG_FREQ0,              0x98,
	/* bw = 101.563 kHz */
	CC_REG_MDMCFG4,            0xC9,
	CC_REG_MDMCFG3,            0x93,
	CC_REG_MDMCFG2,            0x70,
	/* dfch = 49.987 kHz */
	CC_REG_MDMCFG1,            0x20,
	CC_REG_MDMCFG0,            0xF8,
	CC_REG_DEVIATN,            0x34,
	CC_REG_MCSM2,              0x07,
	CC_REG_MCSM1,              0x30,
	CC_REG_MCSM0,              0x18,
	CC_REG_FOCCFG,             0x16,
	CC_REG_BSCFG,              0x6C,
	CC_REG_AGCCTRL2,           0x43,
	CC_REG_AGCCTRL1,           0x40,
	CC_REG_AGCCTRL0,           0x91,
	CC_REG_WOREVT1,            0x87,
	CC_REG_WOREVT0,            0x6B,
	CC_REG_WORCTRL,            0xFB,
	CC_REG_FREND1,             0x56,
	CC_REG_FREND0,             0x10,
	CC_REG_FSCAL3,             0xE9,
	CC_REG_FSCAL2,             0x2A,
	CC_REG_FSCAL1,             0x00,
	CC_REG_FSCAL0,             0x1F,
	CC_REG_RCCTRL1,            0x41,
	CC_REG_RCCTRL0,            0x00,
	CC_REG_FSTEST,             0x59,
	CC_REG_PTEST,              0x7F,
	CC_REG_AGCTEST,            0x3F,
	CC_REG_TEST2,              0x81,
	CC_REG_TEST1,              0x35,
	CC_REG_TEST0,              0x09,
	CC_REG_PARTNUM,            0x00,
	CC_REG_VERSION,            0x04,
	CC_REG_FREQEST,            0x00,
	CC_REG_LQI,                0x00,
	CC_REG_RSSI,               0x00,
	CC_REG_MARCSTATE,          0x00,
	CC_REG_WORTIME1,           0x00,
	CC_REG_WORTIME0,           0x00,
	CC_REG_PKTSTATUS,          0x00,
	CC_REG_VCO_VC_DAC,         0x00,
	CC_REG_TXBYTES,            0x00,
	CC_REG_RXBYTES,            0x00,
	CC_REG_RCCTRL1_STATUS,     0x00,
	CC_REG_RCCTRL0_STATUS,     0x00,
	0xFF,			   0xFF
};

uint8_t dev_cc1101_868mhz_200khz_init_seq[] = {
	CC_REG_IOCFG2,             0x2E,
	CC_REG_IOCFG1,             0x2E,
	CC_REG_IOCFG0,             0x2E,
	CC_REG_FIFOTHR,            0x07,
	CC_REG_SYNC1,              0xD3,
	CC_REG_SYNC0,              0x91,
	CC_REG_PKTLEN,             0xFF,
	CC_REG_PKTCTRL1,           0x04,
	CC_REG_PKTCTRL0,           0x12,
	CC_REG_ADDR,               0x00,
	CC_REG_CHANNR,             0x00,
	CC_REG_FSCTRL1,            0x06,
	CC_REG_FSCTRL0,            0x00,
	/* fc = 835.919678 MHz */
	CC_REG_FREQ2,              0x20,
	CC_REG_FREQ1,              0x26,
	CC_REG_FREQ0,              0x98,
	/* bw = 203.125 kHz */
	CC_REG_MDMCFG4,            0x89,
	CC_REG_MDMCFG3,            0x93,
	CC_REG_MDMCFG2,            0x70,
	/* dfch = 49.987 kHz */
	CC_REG_MDMCFG1,            0x20,
	CC_REG_MDMCFG0,            0xF8,
	CC_REG_DEVIATN,            0x34,
	CC_REG_MCSM2,              0x07,
	CC_REG_MCSM1,              0x30,
	CC_REG_MCSM0,              0x18,
	CC_REG_FOCCFG,             0x16,
	CC_REG_BSCFG,              0x6C,
	CC_REG_AGCCTRL2,           0x43,
	CC_REG_AGCCTRL1,           0x40,
	CC_REG_AGCCTRL0,           0x91,
	CC_REG_WOREVT1,            0x87,
	CC_REG_WOREVT0,            0x6B,
	CC_REG_WORCTRL,            0xFB,
	CC_REG_FREND1,             0x56,
	CC_REG_FREND0,             0x10,
	CC_REG_FSCAL3,             0xE9,
	CC_REG_FSCAL2,             0x2A,
	CC_REG_FSCAL1,             0x00,
	CC_REG_FSCAL0,             0x1F,
	CC_REG_RCCTRL1,            0x41,
	CC_REG_RCCTRL0,            0x00,
	CC_REG_FSTEST,             0x59,
	CC_REG_PTEST,              0x7F,
	CC_REG_AGCTEST,            0x3F,
	CC_REG_TEST2,              0x81,
	CC_REG_TEST1,              0x35,
	CC_REG_TEST0,              0x09,
	CC_REG_PARTNUM,            0x00,
	CC_REG_VERSION,            0x04,
	CC_REG_FREQEST,            0x00,
	CC_REG_LQI,                0x00,
	CC_REG_RSSI,               0x00,
	CC_REG_MARCSTATE,          0x00,
	CC_REG_WORTIME1,           0x00,
	CC_REG_WORTIME0,           0x00,
	CC_REG_PKTSTATUS,          0x00,
	CC_REG_VCO_VC_DAC,         0x00,
	CC_REG_TXBYTES,            0x00,
	CC_REG_RXBYTES,            0x00,
	CC_REG_RCCTRL1_STATUS,     0x00,
	CC_REG_RCCTRL0_STATUS,     0x00,
	0xFF,			   0xFF
};

uint8_t dev_cc1101_868mhz_400khz_init_seq[] = {
	CC_REG_IOCFG2,             0x2E,
	CC_REG_IOCFG1,             0x2E,
	CC_REG_IOCFG0,             0x2E,
	CC_REG_FIFOTHR,            0x07,
	CC_REG_SYNC1,              0xD3,
	CC_REG_SYNC0,              0x91,
	CC_REG_PKTLEN,             0xFF,
	CC_REG_PKTCTRL1,           0x04,
	CC_REG_PKTCTRL0,           0x12,
	CC_REG_ADDR,               0x00,
	CC_REG_CHANNR,             0x00,
	CC_REG_FSCTRL1,            0x06,
	CC_REG_FSCTRL0,            0x00,
	/* fc = 835.919678 MHz */
	CC_REG_FREQ2,              0x20,
	CC_REG_FREQ1,              0x26,
	CC_REG_FREQ0,              0x98,
	/* bw = 406.250 kHz */
	CC_REG_MDMCFG4,            0x49,
	CC_REG_MDMCFG3,            0x93,
	CC_REG_MDMCFG2,            0x70,
	/* dfch = 49.987 kHz */
	CC_REG_MDMCFG1,            0x20,
	CC_REG_MDMCFG0,            0xF8,
	CC_REG_DEVIATN,            0x34,
	CC_REG_MCSM2,              0x07,
	CC_REG_MCSM1,              0x30,
	CC_REG_MCSM0,              0x18,
	CC_REG_FOCCFG,             0x16,
	CC_REG_BSCFG,              0x6C,
	CC_REG_AGCCTRL2,           0x43,
	CC_REG_AGCCTRL1,           0x40,
	CC_REG_AGCCTRL0,           0x91,
	CC_REG_WOREVT1,            0x87,
	CC_REG_WOREVT0,            0x6B,
	CC_REG_WORCTRL,            0xFB,
	CC_REG_FREND1,             0x56,
	CC_REG_FREND0,             0x10,
	CC_REG_FSCAL3,             0xE9,
	CC_REG_FSCAL2,             0x2A,
	CC_REG_FSCAL1,             0x00,
	CC_REG_FSCAL0,             0x1F,
	CC_REG_RCCTRL1,            0x41,
	CC_REG_RCCTRL0,            0x00,
	CC_REG_FSTEST,             0x59,
	CC_REG_PTEST,              0x7F,
	CC_REG_AGCTEST,            0x3F,
	CC_REG_TEST2,              0x81,
	CC_REG_TEST1,              0x35,
	CC_REG_TEST0,              0x09,
	CC_REG_PARTNUM,            0x00,
	CC_REG_VERSION,            0x04,
	CC_REG_FREQEST,            0x00,
	CC_REG_LQI,                0x00,
	CC_REG_RSSI,               0x00,
	CC_REG_MARCSTATE,          0x00,
	CC_REG_WORTIME1,           0x00,
	CC_REG_WORTIME0,           0x00,
	CC_REG_PKTSTATUS,          0x00,
	CC_REG_VCO_VC_DAC,         0x00,
	CC_REG_TXBYTES,            0x00,
	CC_REG_RXBYTES,            0x00,
	CC_REG_RCCTRL1_STATUS,     0x00,
	CC_REG_RCCTRL0_STATUS,     0x00,
	0xFF,			   0xFF
};

const struct spectrum_dev_config dev_cc1101_868mhz_60khz = {
	.name			= "868 MHz ISM, 60 kHz bandwidth",

	.channel_base_hz 	= 862999695,
	.channel_spacing_hz	= 49953,
	.channel_bw_hz		= 60268,
	.channel_num		= 140,

	.channel_time_ms	= 5,

	.priv			= dev_cc1101_868mhz_60khz_init_seq
};

const struct spectrum_dev_config dev_cc1101_868mhz_100khz = {
	.name			= "868 MHz ISM, 100 kHz bandwidth",

	.channel_base_hz 	= 867999729,
	.channel_spacing_hz	= 49987,
	.channel_bw_hz		= 101563,
	.channel_num		= 40,

	.channel_time_ms	= 5,

	.priv			= dev_cc1101_868mhz_100khz_init_seq
};

const struct spectrum_dev_config dev_cc1101_868mhz_200khz = {
	.name			= "868 MHz ISM, 200 kHz bandwidth",

	.channel_base_hz 	= 867999729,
	.channel_spacing_hz	= 49987,
	.channel_bw_hz		= 203125,
	.channel_num		= 40,

	.channel_time_ms	= 5,

	.priv			= dev_cc1101_868mhz_200khz_init_seq
};

const struct spectrum_dev_config dev_cc1101_868mhz_400khz = {
	.name			= "868 MHz ISM, 400 kHz bandwidth",

	.channel_base_hz 	= 867999729,
	.channel_spacing_hz	= 49987,
	.channel_bw_hz		= 406250,
	.channel_num		= 40,

	.channel_time_ms	= 5,

	.priv			= dev_cc1101_868mhz_400khz_init_seq
};

const struct spectrum_dev_config* dev_cc1101_config_list[] = {
	&dev_cc1101_868mhz_60khz,
	&dev_cc1101_868mhz_100khz,
	&dev_cc1101_868mhz_200khz,
	&dev_cc1101_868mhz_400khz
};

const struct spectrum_dev dev_cc1101 = {
	.name = "cc1101",

	.dev_config_list	= dev_cc1101_config_list,
	.dev_config_num		= 4,

	.dev_reset		= dev_cc_reset,
	.dev_setup		= dev_cc_setup,
	.dev_run		= dev_cc_run,

	.priv 			= NULL
};

uint8_t dev_cc2500_2400mhz_400khz_init_seq[] = {
	CC_REG_IOCFG2,         0x2E,
	CC_REG_IOCFG1,         0x2E,
	CC_REG_IOCFG0,         0x2E,
	CC_REG_FIFOTHR,        0x07,
	CC_REG_SYNC1,          0xD3,
	CC_REG_SYNC0,          0x91,
	CC_REG_PKTLEN,         0xFF,
	CC_REG_PKTCTRL1,       0x04,
	CC_REG_PKTCTRL0,       0x32,
	CC_REG_ADDR,           0x00,
	CC_REG_CHANNR,         0x00,
	CC_REG_FSCTRL1,        0x0A,
	CC_REG_FSCTRL0,        0x00,
	CC_REG_FREQ2,          0x58,
	CC_REG_FREQ1,          0xE5,
	CC_REG_FREQ0,          0x68,
	CC_REG_MDMCFG4,        0x4D,
	CC_REG_MDMCFG3,        0x2F,
	CC_REG_MDMCFG2,        0x70,
	CC_REG_MDMCFG1,        0x23,
	CC_REG_MDMCFG0,        0xE5,
	CC_REG_DEVIATN,        0x00,
	CC_REG_MCSM2,          0x07,
	CC_REG_MCSM1,          0x30,
	CC_REG_MCSM0,          0x18,
	CC_REG_FOCCFG,         0x1D,
	CC_REG_BSCFG,          0x1C,
	CC_REG_AGCCTRL2,       0xC7,
	CC_REG_AGCCTRL1,       0x00,
	CC_REG_AGCCTRL0,       0xB0,
	CC_REG_WOREVT1,        0x87,
	CC_REG_WOREVT0,        0x6B,
	CC_REG_WORCTRL,        0xF8,
	CC_REG_FREND1,         0xB6,
	CC_REG_FREND0,         0x10,
	CC_REG_FSCAL3,         0xEA,
	CC_REG_FSCAL2,         0x0A,
	CC_REG_FSCAL1,         0x00,
	CC_REG_FSCAL0,         0x11,
	CC_REG_RCCTRL1,        0x41,
	CC_REG_RCCTRL0,        0x00,
	CC_REG_FSTEST,         0x59,
	CC_REG_PTEST,          0x7F,
	CC_REG_AGCTEST,        0x3F,
	CC_REG_TEST2,          0x88,
	CC_REG_TEST1,          0x31,
	CC_REG_TEST0,          0x0B,
	CC_REG_PARTNUM,        0x80,
	CC_REG_VERSION,        0x03,
	CC_REG_FREQEST,        0x00,
	CC_REG_LQI,            0x00,
	CC_REG_RSSI,           0x00,
	CC_REG_MARCSTATE,      0x00,
	CC_REG_WORTIME1,       0x00,
	CC_REG_WORTIME0,       0x00,
	CC_REG_PKTSTATUS,      0x00,
	CC_REG_VCO_VC_DAC,     0x00,
	CC_REG_TXBYTES,        0x00,
	CC_REG_RXBYTES,        0x00,
	CC_REG_RCCTRL1_STATUS, 0x00,
	CC_REG_RCCTRL0_STATUS, 0x00,
	0xFF,		       0xFF
};

uint8_t dev_cc2500_2400mhz_60khz_init_seq[] = {
	CC_REG_IOCFG2,         0x2E,
	CC_REG_IOCFG1,         0x2E,
	CC_REG_IOCFG0,         0x2E,
	CC_REG_FIFOTHR,        0x07,
	CC_REG_SYNC1,          0xD3,
	CC_REG_SYNC0,          0x91,
	CC_REG_PKTLEN,         0xFF,
	CC_REG_PKTCTRL1,       0x04,
	CC_REG_PKTCTRL0,       0x32,
	CC_REG_ADDR,           0x00,
	CC_REG_CHANNR,         0x00,
	CC_REG_FSCTRL1,        0x0A,
	CC_REG_FSCTRL0,        0x00,
	CC_REG_FREQ2,          0x58,
	CC_REG_FREQ1,          0xE5,
	CC_REG_FREQ0,          0x68,
	CC_REG_MDMCFG4,        0xFD,
	CC_REG_MDMCFG3,        0x2F,
	CC_REG_MDMCFG2,        0x70,
	CC_REG_MDMCFG1,        0x23,
	CC_REG_MDMCFG0,        0xE5,
	CC_REG_DEVIATN,        0x00,
	CC_REG_MCSM2,          0x07,
	CC_REG_MCSM1,          0x30,
	CC_REG_MCSM0,          0x18,
	CC_REG_FOCCFG,         0x1D,
	CC_REG_BSCFG,          0x1C,
	CC_REG_AGCCTRL2,       0xC7,
	CC_REG_AGCCTRL1,       0x00,
	CC_REG_AGCCTRL0,       0xB0,
	CC_REG_WOREVT1,        0x87,
	CC_REG_WOREVT0,        0x6B,
	CC_REG_WORCTRL,        0xF8,
	CC_REG_FREND1,         0xB6,
	CC_REG_FREND0,         0x10,
	CC_REG_FSCAL3,         0xEA,
	CC_REG_FSCAL2,         0x0A,
	CC_REG_FSCAL1,         0x00,
	CC_REG_FSCAL0,         0x11,
	CC_REG_RCCTRL1,        0x41,
	CC_REG_RCCTRL0,        0x00,
	CC_REG_FSTEST,         0x59,
	CC_REG_PTEST,          0x7F,
	CC_REG_AGCTEST,        0x3F,
	CC_REG_TEST2,          0x88,
	CC_REG_TEST1,          0x31,
	CC_REG_TEST0,          0x0B,
	CC_REG_PARTNUM,        0x80,
	CC_REG_VERSION,        0x03,
	CC_REG_FREQEST,        0x00,
	CC_REG_LQI,            0x00,
	CC_REG_RSSI,           0x00,
	CC_REG_MARCSTATE,      0x00,
	CC_REG_WORTIME1,       0x00,
	CC_REG_WORTIME0,       0x00,
	CC_REG_PKTSTATUS,      0x00,
	CC_REG_VCO_VC_DAC,     0x00,
	CC_REG_TXBYTES,        0x00,
	CC_REG_RXBYTES,        0x00,
	CC_REG_RCCTRL1_STATUS, 0x00,
	CC_REG_RCCTRL0_STATUS, 0x00,
	0xFF,		       0xFF
};

const struct spectrum_dev_config dev_cc2500_2400mhz_60khz = {
	.name			= "2.4 GHz ISM, 60 kHz bandwidth",

	.channel_base_hz 	= 2399999692ll,
	.channel_spacing_hz	= 399595,
	.channel_bw_hz		= 60263,
	.channel_num		= 209,

	.channel_time_ms	= 5,

	.priv			= dev_cc2500_2400mhz_60khz_init_seq
};

const struct spectrum_dev_config dev_cc2500_2400mhz_400khz = {
	.name			= "2.4 GHz ISM, 400 kHz bandwidth",

	.channel_base_hz 	= 2399999692ll,
	.channel_spacing_hz	= 399595,
	.channel_bw_hz		= 421841,
	.channel_num		= 209,

	.channel_time_ms	= 5,

	.priv			= dev_cc2500_2400mhz_400khz_init_seq
};

const struct spectrum_dev_config* dev_cc2500_config_list[] = {
	&dev_cc2500_2400mhz_400khz,
	&dev_cc2500_2400mhz_60khz
};

const struct spectrum_dev dev_cc2500 = {
	.name = "cc2500",

	.dev_config_list	= dev_cc2500_config_list,
	.dev_config_num		= 2,

	.dev_reset		= dev_cc_reset,
	.dev_setup		= dev_cc_setup,
	.dev_run		= dev_cc_run,

	.priv 			= NULL
};

int dev_cc_register(void)
{
#if defined(MODEL_SNR_TRX_868) || defined(MODEL_SNE_ISMTV_868)
	return spectrum_add_dev(&dev_cc1101);
#endif

#if defined(MODEL_SNR_TRX_2400) || defined(MODEL_SNE_ISMTV_2400)
	return spectrum_add_dev(&dev_cc2500);
#endif
}

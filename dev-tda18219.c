#include <stdio.h>
#include <stdlib.h>
#include <libopencm3/stm32/f1/gpio.h>
#include <libopencm3/stm32/f1/rcc.h>
#include <libopencm3/stm32/f1/rtc.h>
#include <libopencm3/stm32/i2c.h>
#include <tda18219/tda18219.h>
#include <tda18219/tda18219regs.h>

#include "spectrum.h"

static void setup_stm32f1_peripherals(void)
{
	rcc_peripheral_enable_clock(&RCC_APB1ENR, 
			RCC_APB1ENR_I2C1EN);


	/* GPIO pin for I2C1 SCL, SDA */
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,
			GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN, GPIO6);
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,
			GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN, GPIO7);

	/* GPIO pin for TDA18219 IRQ */
	gpio_set_mode(GPIOA, GPIO_MODE_INPUT,
			GPIO_CNF_INPUT_FLOAT, GPIO7);

	/* GPIO pin for TDA18219 IF AGC */
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ,
			GPIO_CNF_OUTPUT_PUSHPULL, GPIO4);
	/* Set to lowest gain for now */
	gpio_clear(GPIOA, GPIO4);


	/* Setup I2C */
	i2c_peripheral_disable(I2C1);

	/* 400 kHz - I2C Fast Mode */
	i2c_set_clock_frequency(I2C1, I2C_CR2_FREQ_24MHZ);
	i2c_set_fast_mode(I2C1);
	/* 400 kHz */
	i2c_set_ccr(I2C1, 0x14);
	/* 300 ns rise time */
	i2c_set_trise(I2C1, 0x08);

	i2c_peripheral_enable(I2C1);
}


uint8_t tda18219_read_reg(uint8_t reg)
{
	uint32_t __attribute__((unused)) reg32;

	/* Send START condition. */
	i2c_send_start(I2C1);

	/* Waiting for START is send and switched to master mode. */
	while (!((I2C_SR1(I2C1) & I2C_SR1_SB)
		& (I2C_SR2(I2C1) & (I2C_SR2_MSL | I2C_SR2_BUSY))));

	/* Say to what address we want to talk to. */
	/* Yes, WRITE is correct - for selecting register in STTS75. */
	i2c_send_7bit_address(I2C1, TDA18219_I2C_ADDR, I2C_WRITE);

	/* Waiting for address is transferred. */
	while (!(I2C_SR1(I2C1) & I2C_SR1_ADDR));

	/* Cleaning ADDR condition sequence. */
	reg32 = I2C_SR2(I2C1);

	i2c_send_data(I2C1, reg);
	while (!(I2C_SR1(I2C1) & (I2C_SR1_BTF | I2C_SR1_TxE)));

	/* Send re-START condition. */
	i2c_send_start(I2C1);

	/* Waiting for START is send and switched to master mode. */
	while (!((I2C_SR1(I2C1) & I2C_SR1_SB)
		& (I2C_SR2(I2C1) & (I2C_SR2_MSL | I2C_SR2_BUSY))));

	/* Say to what address we want to talk to. */
	i2c_send_7bit_address(I2C1, TDA18219_I2C_ADDR, I2C_READ); 

	I2C_CR1(I2C1) &= ~I2C_CR1_ACK;

	/* Waiting for address is transferred. */
	while (!(I2C_SR1(I2C1) & I2C_SR1_ADDR));

	/* Cleaning ADDR condition sequence. */
	reg32 = I2C_SR2(I2C1);

	i2c_send_stop(I2C1);

	while (!(I2C_SR1(I2C1) & I2C_SR1_RxNE));

	uint8_t value = I2C_DR(I2C1);
	return value;
}

void tda18219_write_reg(uint8_t reg, uint8_t value)
{
	uint32_t __attribute__((unused)) reg32;

	/* Send START condition. */
	i2c_send_start(I2C1);

	/* Waiting for START is send and switched to master mode. */
	while (!((I2C_SR1(I2C1) & I2C_SR1_SB)
		& (I2C_SR2(I2C1) & (I2C_SR2_MSL | I2C_SR2_BUSY))));

	/* Say to what address we want to talk to. */
	/* Yes, WRITE is correct - for selecting register in STTS75. */
	i2c_send_7bit_address(I2C1, TDA18219_I2C_ADDR, I2C_WRITE);

	/* Waiting for address is transferred. */
	while (!(I2C_SR1(I2C1) & I2C_SR1_ADDR));

	/* Cleaning ADDR condition sequence. */
	reg32 = I2C_SR2(I2C1);

	i2c_send_data(I2C1, reg);
	while (!(I2C_SR1(I2C1) & I2C_SR1_TxE));

	i2c_send_data(I2C1, value);
	while (!(I2C_SR1(I2C1) & (I2C_SR1_BTF | I2C_SR1_TxE)));

	i2c_send_stop(I2C1);
}

void tda18219_wait_irq(void)
{
	while(!gpio_get(GPIOA, GPIO7));
}


int dev_tda18219_reset(void* priv) 
{
	setup_stm32f1_peripherals();
	tda18219_init();
	return E_SPECTRUM_OK;
}

int dev_tda18219_setup(void* priv, const struct spectrum_sweep_config* sweep_config) 
{
	tda18219_set_standard((struct tda18219_standard*) sweep_config->dev_config->priv);
	return E_SPECTRUM_OK;
}

int dev_tda18219_run(void* priv, const struct spectrum_sweep_config* sweep_config)
{
	int r;
	short int *data;

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
			int freq = sweep_config->dev_config->channel_base_hz + \
				   	sweep_config->dev_config->channel_spacing_hz * ch;
			tda18219_set_frequency((struct tda18219_standard*) sweep_config->dev_config->priv,
					freq);

			int rssi_dbuv = tda18219_get_input_power();
			// P [dBm] = U [dBuV] - 90 - 10 log 75 ohm
			int rssi_dbm_100 = rssi_dbuv * 100 - 9000 - 1875;

			//uint8_t rssi_u = tda18219_read_reg(TDA18219_POWER_1);
			//int rssi_dbm_100 = rssi_u * 25 + 3600 - 9000 - 1875;

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

void dev_tda18219_print_status(void)
{
	struct tda18219_status status;
	tda18219_get_status(&status);

	printf("IC          : TDA18219HN\n\n");
	printf("Ident       : %04x\n", status.ident);
	printf("Major rev   : %d\n", status.major_rev);
	printf("Minor rev   : %d\n\n", status.minor_rev);
	printf("Temperature : %d C\n", status.temperature);
	printf("Power-on    : %s\n", status.por_flag ? "true" : "false");
	printf("LO lock     : %s\n", status.lo_lock  ? "true" : "false");
	printf("Sleep mode  : %s\n", status.sm ? "true" : "false");
	printf("Sleep LNA   : %s\n\n", status.sm_lna ? "true" : "false");

	int n;
	for(n = 0; n < 12; n++) {
		printf("RF cal %02d   : %d%s\n", n, status.calibration_ncaps[n], 
				status.calibration_error[n] ? " (error)" : "");
	}
}

const struct spectrum_dev_config dev_tda18219_dvbt_1700khz = {
	.name			= "DVB-T 1.7 MHz",

	// UHF: 470 MHz to 862 MHz
	.channel_base_hz 	= 470000000,
	.channel_spacing_hz	= 1000,
	.channel_bw_hz		= 1700000,
	.channel_num		= 392000,

	.channel_time_ms	= 50,

	.priv			= &tda18219_standard_dvbt_1700khz
};

const struct spectrum_dev_config* dev_tda18219_config_list[] = { &dev_tda18219_dvbt_1700khz };

const struct spectrum_dev dev_tda18219 = {
	.name = "tda18219hn",

	.dev_config_list	= dev_tda18219_config_list,
	.dev_config_num		= 1,

	.dev_reset		= dev_tda18219_reset,
	.dev_setup		= dev_tda18219_setup,
	.dev_run		= dev_tda18219_run,

	.priv 			= NULL
};

int dev_tda18219_register(void)
{
	return spectrum_add_dev(&dev_tda18219);
}

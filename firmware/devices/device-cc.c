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

/* Authors:	Ales Verbic
 * 		Zoltan Padrah
 * 		Tomaz Solc, <tomaz.solc@ijs.si> */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include "average.h"
#include "cc.h"
#include "rtc.h"
#include "task.h"
#include "timer.h"

#include "device-cc.h"

static struct vss_task* current_task = NULL;

static int vss_device_cc_init(void)
{
	int r;

	r = vss_timer_init();
	if(r) return r;

	r = vss_rtc_init();
	if(r) return r;

	r = vss_cc_init();
	if(r) return r;

	return VSS_OK;
}

static int dev_cc_status_ic(char* buffer, size_t len, const char* ic)
{
	int r;

	uint8_t partnum, version;

	r = vss_cc_read_reg(CC_REG_PARTNUM, &partnum);
	if(r) return r;

	r = vss_cc_read_reg(CC_REG_VERSION, &version);
	if(r) return r;

	int wlen = snprintf(buffer, len,
			"IC          : %s\n"
			"Part number : %02hhx\n"
			"Version     : %02hhx\n",
			ic, partnum, version);
	if(wlen >= (int) len) return VSS_TOO_MANY;
		
	return VSS_OK;
}

static int dev_cc1101_status(void* priv __attribute__((unused)), char* buffer, size_t len)
{
	return dev_cc_status_ic(buffer, len, "CC1101");
}

static int dev_cc2500_status(void* priv __attribute__((unused)), char* buffer, size_t len)
{
	return dev_cc_status_ic(buffer, len, "CC2500");
}

static int dev_cc_setup(const uint8_t* init_seq)
{
	int r;

	r = vss_cc_strobe(CC_STROBE_SIDLE);
	if(r) return r;

	r = vss_cc_wait_state(CC_MARCSTATE_IDLE);
	if(r) return r;

	int n;
	for(n = 0; init_seq[n] != 0xff; n += 2) {
		uint8_t reg = init_seq[n];
		uint8_t value = init_seq[n+1];

		r = vss_cc_write_reg(reg, value);
		if(r) return r;
	}

	return VSS_OK;
}

static int dev_cc_prepare_measurement(struct vss_task* task)
{
	unsigned int ch = vss_task_get_channel(task);

	int r;

	r = vss_cc_strobe(CC_STROBE_SIDLE);
	if(r) return r;

	r = vss_cc_wait_state(CC_MARCSTATE_IDLE);
	if(r) return r;

	r = vss_cc_write_reg(CC_REG_CHANNR, ch);
	if(r) return r;

	r = vss_cc_strobe(CC_STROBE_SRX);
	if(r) return r;

	r = vss_cc_wait_state(CC_MARCSTATE_RX);
	if(r) return r;

	vss_timer_schedule(2);

	return VSS_OK;
}

static void dev_cc_take_measurement(struct vss_task* task)
{
	int r;
	int8_t reg;

	unsigned n;
	unsigned n_average = vss_task_get_n_average(task);
	power_t buffer[n_average];

	for(n = 0; n < n_average; n++) {
		r = vss_cc_read_reg(CC_REG_RSSI, (uint8_t*) &reg);
		if(r) {
			vss_task_set_error(task,
					"vss_cc_read_reg for RSSI returned an error");
			current_task = NULL;
			return;
		}

		power_t rssi_dbm_100 = -5920 + reg * 50;
		buffer[n] = rssi_dbm_100;
	}

	power_t rssi_dbm_100 = vss_average(buffer, n_average);

	if(vss_task_insert_sweep(task, rssi_dbm_100, vss_rtc_read()) == VSS_OK) {
		r = dev_cc_prepare_measurement(task);
		if(r) {
			vss_task_set_error(task,
				"dev_cc_prepare_measurement() returned an error");
			current_task = NULL;
			return;
		}
	} else {
		current_task = NULL;
	}
}

static int dev_cc_run(void* priv __attribute__((unused)), struct vss_task* task)
{
	if(current_task != NULL) {
		return VSS_TOO_MANY;
	}
	current_task = task;

	int r;
	r = dev_cc_setup(task->sweep_config->device_config->priv);
	if(r) return r;

	r = vss_rtc_reset();
	if(r) return r;

	r = dev_cc_prepare_measurement(task);
	if(r) return r;

	return VSS_OK;
}

void vss_device_cc_timer_isr(void)
{
	dev_cc_take_measurement(current_task);
}

static const struct vss_device device_cc1101 = {
	.name = "cc1101",

	.run			= dev_cc_run,
	.resume			= NULL,
	.status			= dev_cc1101_status,

	.supports_task_baseband	= 0,

	.priv 			= NULL
};

static uint8_t dev_cc1101_868mhz_60khz_init_seq[] = {
	/* Channel spacing = 49.953461
	 * RX filter BW = 60.267857
	 * Base frequency = 862.999695
	 * Xtal frequency = 27.000000 */
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

static uint8_t dev_cc1101_868mhz_100khz_init_seq[] = {
	/* Channel spacing = 49.953461
	 * RX filter BW = 105.468750
	 * Base frequency = 867.999985
	 * Xtal frequency = 27.000000 */
	CC_REG_IOCFG2,             0x2E,
	CC_REG_IOCFG1,             0x2E,
	CC_REG_IOCFG0,             0x2E,
	CC_REG_FIFOTHR,            0x47,
	CC_REG_SYNC1,              0xD3,
	CC_REG_SYNC0,              0x91,
	CC_REG_PKTLEN,             0xFF,
	CC_REG_PKTCTRL1,           0x04,
	CC_REG_PKTCTRL0,           0x12,
	CC_REG_ADDR,               0x00,
	CC_REG_CHANNR,             0x00,
	CC_REG_FSCTRL1,            0x06,
	CC_REG_FSCTRL0,            0x00,
	CC_REG_FREQ2,              0x20,
	CC_REG_FREQ1,              0x25,
	CC_REG_FREQ0,              0xED,
	CC_REG_MDMCFG4,            0xC9,
	CC_REG_MDMCFG3,            0x93,
	CC_REG_MDMCFG2,            0x70,
	CC_REG_MDMCFG1,            0x20,
	CC_REG_MDMCFG0,            0xE5,
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

static uint8_t dev_cc1101_868mhz_200khz_init_seq[] = {
	/* Channel spacing = 49.953461
	 * RX filter BW = 210.937500
	 * Base frequency = 867.999985
	 * Xtal frequency = 27.000000 */
	CC_REG_IOCFG2,             0x2E,
	CC_REG_IOCFG1,             0x2E,
	CC_REG_IOCFG0,             0x2E,
	CC_REG_FIFOTHR,            0x47,
	CC_REG_SYNC1,              0xD3,
	CC_REG_SYNC0,              0x91,
	CC_REG_PKTLEN,             0xFF,
	CC_REG_PKTCTRL1,           0x04,
	CC_REG_PKTCTRL0,           0x12,
	CC_REG_ADDR,               0x00,
	CC_REG_CHANNR,             0x00,
	CC_REG_FSCTRL1,            0x06,
	CC_REG_FSCTRL0,            0x00,
	CC_REG_FREQ2,              0x20,
	CC_REG_FREQ1,              0x25,
	CC_REG_FREQ0,              0xED,
	CC_REG_MDMCFG4,            0x89,
	CC_REG_MDMCFG3,            0x93,
	CC_REG_MDMCFG2,            0x70,
	CC_REG_MDMCFG1,            0x20,
	CC_REG_MDMCFG0,            0xE5,
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

static uint8_t dev_cc1101_868mhz_400khz_init_seq[] = {
	/* Channel spacing = 49.953461
	 * RX filter BW = 421.875000
	 * Base frequency = 867.999985
	 * Xtal frequency = 27.000000 */
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
	CC_REG_FREQ2,              0x20,
	CC_REG_FREQ1,              0x25,
	CC_REG_FREQ0,              0xED,
	CC_REG_MDMCFG4,            0x49,
	CC_REG_MDMCFG3,            0x93,
	CC_REG_MDMCFG2,            0x70,
	CC_REG_MDMCFG1,            0x20,
	CC_REG_MDMCFG0,            0xE5,
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

static uint8_t dev_cc1101_868mhz_400khz_200khz_init_seq[] = {
	/* Base frequency = 868.299911
	 * Channel spacing = 199.813843
	 * RX filter BW = 421.875000
	 * Xtal frequency = 27.000000 */
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
	CC_REG_FREQ2,              0x20,
	CC_REG_FREQ1,              0x28,
	CC_REG_FREQ0,              0xC5,
	CC_REG_MDMCFG4,            0x49,
	CC_REG_MDMCFG3,            0x93,
	CC_REG_MDMCFG2,            0x70,
	CC_REG_MDMCFG1,            0x22,
	CC_REG_MDMCFG0,            0xE5,
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
	0xFF,		 	   0xFF
};

static uint8_t dev_cc1101_868mhz_800khz_200khz_init_seq[] = {
	/* Base frequency = 867.999985
	 * Channel spacing = 199.813843
	 * RX filter BW = 843.750000
	 * Xtal frequency = 27.000000 */
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
	CC_REG_FREQ2,              0x20,
	CC_REG_FREQ1,              0x25,
	CC_REG_FREQ0,              0xED,
	CC_REG_MDMCFG4,            0x09,
	CC_REG_MDMCFG3,            0x84,
	CC_REG_MDMCFG2,            0x70,
	CC_REG_MDMCFG1,            0x22,
	CC_REG_MDMCFG0,            0xE5,
	CC_REG_DEVIATN,            0x67,
	CC_REG_MCSM2,              0x07,
	CC_REG_MCSM1,              0x30,
	CC_REG_MCSM0,              0x18,
	CC_REG_FOCCFG,             0x16,
	CC_REG_BSCFG,              0x6C,
	CC_REG_AGCCTRL2,           0x03,
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
	CC_REG_TEST2,              0x88,
	CC_REG_TEST1,              0x31,
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

static uint8_t dev_cc1101_905mhz_400khz_400khz_init_seq[] = {
	/* Base frequency = 905.999634
	 * Channel spacing = 399.627686
	 * RX filter BW = 421.875000
	 * Xtal frequency = 27.000000 */
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
	CC_REG_FREQ2,              0x21,
	CC_REG_FREQ1,              0x8E,
	CC_REG_FREQ0,              0x38,
	CC_REG_MDMCFG4,            0x4A,
	CC_REG_MDMCFG3,            0x84,
	CC_REG_MDMCFG2,            0x70,
	CC_REG_MDMCFG1,            0x23,
	CC_REG_MDMCFG0,            0xE5,
	CC_REG_DEVIATN,            0x67,
	CC_REG_MCSM2,              0x07,
	CC_REG_MCSM1,              0x30,
	CC_REG_MCSM0,              0x18,
	CC_REG_FOCCFG,             0x16,
	CC_REG_BSCFG,              0x6C,
	CC_REG_AGCCTRL2,           0x03,
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
	CC_REG_TEST2,              0x88,
	CC_REG_TEST1,              0x31,
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
	0xFF,		 	   0xFF
};

static uint8_t dev_cc1101_905mhz_800khz_400khz_init_seq[] = {
	/* Base frequency = 905.999634
	 * Channel spacing = 399.627686
	 * RX filter BW = 843.750000
	 * Xtal frequency = 27.000000 */
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
	CC_REG_FREQ2,              0x21,
	CC_REG_FREQ1,              0x8E,
	CC_REG_FREQ0,              0x38,
	CC_REG_MDMCFG4,            0x0A,
	CC_REG_MDMCFG3,            0x84,
	CC_REG_MDMCFG2,            0x70,
	CC_REG_MDMCFG1,            0x23,
	CC_REG_MDMCFG0,            0xE5,
	CC_REG_DEVIATN,            0x67,
	CC_REG_MCSM2,              0x07,
	CC_REG_MCSM1,              0x30,
	CC_REG_MCSM0,              0x18,
	CC_REG_FOCCFG,             0x16,
	CC_REG_BSCFG,              0x6C,
	CC_REG_AGCCTRL2,           0x03,
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
	CC_REG_TEST2,              0x88,
	CC_REG_TEST1,              0x31,
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
	0xFF,		 	   0xFF
};

static const struct vss_device_config dev_cc1101_868mhz_60khz = {
	.name			= "868 MHz ISM, 60 kHz bandwidth",

	.device			= &device_cc1101,

	.channel_base_hz 	= 862999695,
	.channel_spacing_hz	= 49953,
	.channel_bw_hz		= 60268,
	.channel_num		= 140,

	.channel_time_ms	= 5,

	.priv			= dev_cc1101_868mhz_60khz_init_seq
};

static const struct vss_device_config dev_cc1101_868mhz_100khz = {
	.name			= "868 MHz ISM, 100 kHz bandwidth",

	.device			= &device_cc1101,

	.channel_base_hz 	= 867999985,
	.channel_spacing_hz	= 49953,
	.channel_bw_hz		= 105469,
	.channel_num		= 40,

	.channel_time_ms	= 5,

	.priv			= dev_cc1101_868mhz_100khz_init_seq
};

static const struct vss_device_config dev_cc1101_868mhz_200khz = {
	.name			= "868 MHz ISM, 200 kHz bandwidth",

	.device			= &device_cc1101,

	.channel_base_hz 	= 867999985,
	.channel_spacing_hz	= 49953,
	.channel_bw_hz		= 210938,
	.channel_num		= 40,

	.channel_time_ms	= 5,

	.priv			= dev_cc1101_868mhz_200khz_init_seq
};

static const struct vss_device_config dev_cc1101_868mhz_400khz = {
	.name			= "868 MHz ISM, 400 kHz bandwidth",

	.device			= &device_cc1101,

	.channel_base_hz 	= 867999985,
	.channel_spacing_hz	= 49953,
	.channel_bw_hz		= 421875,
	.channel_num		= 40,

	.channel_time_ms	= 5,

	.priv			= dev_cc1101_868mhz_400khz_init_seq
};

static const struct vss_device_config dev_cc1101_868mhz_400khz_200khz = {
	.name			= "868 MHz ISM, 400 kHz bandwidth, 200 kHz spacing",

	.device			= &device_cc1101,

	.channel_base_hz 	= 868299911,
	.channel_spacing_hz	= 199814,
	.channel_bw_hz		= 421875,
	.channel_num		= 256,

	.channel_time_ms	= 5,

	.priv			= dev_cc1101_868mhz_400khz_200khz_init_seq
};

static const struct vss_device_config dev_cc1101_868mhz_800khz_200khz = {
	.name			= "868 MHz ISM, 800 kHz bandwidth, 200 kHz spacing",

	.device			= &device_cc1101,

	.channel_base_hz 	= 867999985,
	.channel_spacing_hz	= 199814,
	.channel_bw_hz		= 843750,
	.channel_num		= 256,

	.channel_time_ms	= 5,

	.priv			= dev_cc1101_868mhz_800khz_200khz_init_seq
};

static const struct vss_device_config dev_cc1101_905mhz_400khz_400khz = {
	.name			= "905 MHz, 400 kHz bandwidth, 400 kHz spacing",

	.device			= &device_cc1101,

	.channel_base_hz 	= 905999634,
	.channel_spacing_hz	= 399628,
	.channel_bw_hz		= 421875,
	.channel_num		= 256,

	.channel_time_ms	= 5,

	.priv			= dev_cc1101_905mhz_400khz_400khz_init_seq
};

static const struct vss_device_config dev_cc1101_905mhz_800khz_400khz = {
	.name			= "905 MHz, 800 kHz bandwidth, 400 kHz spacing",

	.device			= &device_cc1101,

	.channel_base_hz 	= 905999634,
	.channel_spacing_hz	= 399628,
	.channel_bw_hz		= 843750,
	.channel_num		= 256,

	.channel_time_ms	= 5,

	.priv			= dev_cc1101_905mhz_800khz_400khz_init_seq
};

static const struct vss_device device_cc2500 = {
	.name = "cc2500",

	.run			= dev_cc_run,
	.resume			= NULL,
	.status			= dev_cc2500_status,

	.supports_task_baseband	= 0,

	.priv 			= NULL
};

static uint8_t dev_cc2500_2400mhz_400khz_init_seq[] = {
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
	CC_REG_FREQ1,          0xE3,
	CC_REG_FREQ0,          0x8E,
	CC_REG_MDMCFG4,        0x4D,
	CC_REG_MDMCFG3,        0x2F,
	CC_REG_MDMCFG2,        0x70,
	CC_REG_MDMCFG1,        0x03,
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

static uint8_t dev_cc2500_2400mhz_60khz_init_seq[] = {
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
	CC_REG_FREQ1,          0xE3,
	CC_REG_FREQ0,          0x8E,
	CC_REG_MDMCFG4,        0xFD,
	CC_REG_MDMCFG3,        0x2F,
	CC_REG_MDMCFG2,        0x70,
	CC_REG_MDMCFG1,        0x03,
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

static const struct vss_device_config dev_cc2500_2400mhz_60khz = {
	.name			= "2.4 GHz ISM, 60 kHz bandwidth",

	.device			= &device_cc2500,

	.channel_base_hz 	= 2399999908ll,
	.channel_spacing_hz	= 399628,
	.channel_bw_hz		= 60268,
	.channel_num		= 209,

	.channel_time_ms	= 5,

	.priv			= dev_cc2500_2400mhz_60khz_init_seq
};

static const struct vss_device_config dev_cc2500_2400mhz_400khz = {
	.name			= "2.4 GHz ISM, 400 kHz bandwidth",

	.device			= &device_cc2500,

	.channel_base_hz 	= 2399999908ll,
	.channel_spacing_hz	= 399628,
	.channel_bw_hz		= 421875,
	.channel_num		= 209,

	.channel_time_ms	= 5,

	.priv			= dev_cc2500_2400mhz_400khz_init_seq
};

int vss_device_cc_register(void)
{
	int r;
	
	r = vss_device_cc_init();
	if(r) return r;

#if defined(MODEL_SNR_TRX_868) || defined(MODEL_SNE_ISMTV_868)
	vss_device_config_add(&dev_cc1101_868mhz_60khz);
	vss_device_config_add(&dev_cc1101_868mhz_100khz);
	vss_device_config_add(&dev_cc1101_868mhz_200khz);
	vss_device_config_add(&dev_cc1101_868mhz_400khz);
	vss_device_config_add(&dev_cc1101_868mhz_400khz_200khz);
	vss_device_config_add(&dev_cc1101_868mhz_800khz_200khz);
	vss_device_config_add(&dev_cc1101_905mhz_400khz_400khz);
	vss_device_config_add(&dev_cc1101_905mhz_800khz_400khz);
#endif

#if defined(MODEL_SNR_TRX_2400) || defined(MODEL_SNE_ISMTV_2400)
	vss_device_config_add(&dev_cc2500_2400mhz_60khz);
	vss_device_config_add(&dev_cc2500_2400mhz_400khz);
#endif

	return VSS_OK;
}

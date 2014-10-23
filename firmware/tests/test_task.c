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
#include <string.h>

#include "unity.h"
#include "task.h"

#define buffer_data_len 1024

static struct vss_task run;
static power_t buffer_data[buffer_data_len];

int run_f(void* priv, struct vss_task* run)
{
	return VSS_OK;
}

static int resume_called = 0;

int resume_f(void* priv, struct vss_task* run)
{
	resume_called++;
	return VSS_OK;
}

static const struct vss_device device = {
	.run = run_f,
	.resume = resume_f
};

static const struct vss_device_config device_config = {
	.device = &device
};

static const struct vss_sweep_config sweep_config = {
	.device_config = &device_config,

	.channel_start = 0,
	.channel_stop = 10,
	.channel_step = 1,
	.n_average = 10
};

static const struct vss_sweep_config sample_config = {
	.device_config = &device_config,

	.channel_start = 0,
	.channel_stop = 0,
	.channel_step = 0,
	.n_average = 10
};

void setUp(void)
{
}

void tearDown(void)
{
}

void test_init(void)
{
	vss_task_init(&run, VSS_TASK_SWEEP, &sweep_config, 1, buffer_data);
	TEST_ASSERT_EQUAL(VSS_DEVICE_RUN_NEW, vss_task_get_state(&run));
}

void test_start(void)
{
	vss_task_init(&run, VSS_TASK_SWEEP, &sweep_config, 1, buffer_data);
	vss_task_start(&run);
	TEST_ASSERT_EQUAL(VSS_DEVICE_RUN_RUNNING, vss_task_get_state(&run));
}

void test_single_run(void)
{
	const power_t v = 0x70fe;

	vss_task_init(&run, VSS_TASK_SWEEP, &sweep_config, 2, buffer_data);
	vss_task_start(&run);

	int cnt = 0;
	while(1) {
		int ch = vss_task_get_channel(&run);
		TEST_ASSERT_EQUAL(cnt%10, ch);
		int r = vss_task_insert_sweep(&run, v, 0xdeadbeef);
		if(r == VSS_OK) {
			cnt++;
		} else if(r == VSS_STOP) {
			cnt++;
			break;
		} else {
			break;
		}
	}

	TEST_ASSERT_EQUAL(20, cnt);
}

void test_single_run_block(void)
{
	vss_task_init(&run, VSS_TASK_SWEEP, &sweep_config, 2, buffer_data);
	vss_task_start(&run);

	int cnt = 0;
	while(1) {
		int ch = vss_task_get_channel(&run);
		TEST_ASSERT_EQUAL(cnt%10, ch);

		power_t *wptr;
		int r = vss_task_reserve_sample(&run, &wptr, 0xdeadbeef);
		TEST_ASSERT_EQUAL(VSS_OK, r);

		memset(wptr, 0x01, sweep_config.n_average*sizeof(*wptr));

		r = vss_task_write_sample(&run);
		if(r == VSS_OK) {
			cnt++;
		} else if(r == VSS_STOP) {
			cnt++;
			break;
		} else {
			break;
		}
	}

	TEST_ASSERT_EQUAL(20, cnt);
}


void test_infinite_run(void)
{
	const power_t v = 0x70fe;

	vss_task_init(&run, VSS_TASK_SWEEP, &sweep_config, -1, buffer_data);
	vss_task_start(&run);

	int cnt, r;
	for(cnt = 0; cnt < 100; cnt++) {
		r = vss_task_insert_sweep(&run, v, 0xdeadbeef);
		TEST_ASSERT_EQUAL(VSS_OK, r);
	}

	vss_task_stop(&run);

	for(cnt = 0; cnt < 100; cnt++) {
		r = vss_task_insert_sweep(&run, v, 0xdeadbeef);
		if(r != VSS_OK) break;
	}

	TEST_ASSERT_EQUAL(VSS_STOP, r);
	TEST_ASSERT_TRUE(cnt <= 10);
	TEST_ASSERT_EQUAL(VSS_DEVICE_RUN_FINISHED, vss_task_get_state(&run));
}

void test_read(void)
{
	const power_t v = 0x70fe;

	vss_task_init(&run, VSS_TASK_SWEEP, &sweep_config, -1, buffer_data);
	vss_task_start(&run);

	int cnt;
	for(cnt = 0; cnt < 10; cnt++) {
		vss_task_insert_sweep(&run, v, 0xdeadbeef);
	}

	struct vss_task_read_result ctx;
	int r = vss_task_read(&run, &ctx);

	TEST_ASSERT_EQUAL(VSS_OK, r);

	TEST_ASSERT_EQUAL(0xdeadbeef, ctx.block->timestamp);
	TEST_ASSERT_EQUAL(0, ctx.block->channel);

	unsigned int channel;
	uint32_t timestamp;
	power_t power;

	cnt = 0;
	while(vss_task_read_parse(&ctx, &timestamp, &channel, &power) == VSS_OK) {
		TEST_ASSERT_EQUAL(0xdeadbeef, timestamp);
		TEST_ASSERT_EQUAL(v, power);
		TEST_ASSERT_EQUAL(cnt, channel);
		cnt++;
	}

	TEST_ASSERT_EQUAL(10, cnt);
}

void test_read_sample(void)
{
	vss_task_init(&run, VSS_TASK_SAMPLE, &sample_config, -1, buffer_data);
	vss_task_start(&run);

	int cnt;
	for(cnt = 0; cnt < 2; cnt++) {
		power_t *wptr;
		vss_task_reserve_sample(&run, &wptr, 0xdeadbeef);
		memset(wptr, 0x01, sweep_config.n_average*sizeof(*wptr));
		vss_task_write_sample(&run);
	}

	struct vss_task_read_result ctx;
	int r = vss_task_read(&run, &ctx);

	TEST_ASSERT_EQUAL(VSS_OK, r);
	TEST_ASSERT_EQUAL(0xdeadbeef, ctx.block->timestamp);
	TEST_ASSERT_EQUAL(0, ctx.block->channel);

	unsigned int channel;
	uint32_t timestamp;
	power_t power;

	cnt = 0;
	while(vss_task_read_parse(&ctx, &timestamp, &channel, &power) == VSS_OK) {
		TEST_ASSERT_EQUAL(0xdeadbeef, timestamp);
		TEST_ASSERT_EQUAL(0x0101, power);
		TEST_ASSERT_EQUAL(0, channel);
		cnt++;
	}

	TEST_ASSERT_EQUAL(10, cnt);
}

void test_get_channel(void)
{
	vss_task_init(&run, VSS_TASK_SWEEP, &sweep_config, 1, buffer_data);
	vss_task_start(&run);

	int channel = vss_task_get_channel(&run);
	TEST_ASSERT_EQUAL(sweep_config.channel_start, channel);

	vss_task_insert_sweep(&run, 0, 0);

	channel = vss_task_get_channel(&run);
	TEST_ASSERT_EQUAL(sweep_config.channel_start + sweep_config.channel_step, channel);
}

void test_get_average(void)
{
	vss_task_init(&run, VSS_TASK_SWEEP, &sweep_config, 1, buffer_data);
	vss_task_start(&run);

	int n_average = vss_task_get_n_average(&run);
	TEST_ASSERT_EQUAL(10, n_average);
}

void test_set_error(void)
{
	const char* msg = "Test error message";

	vss_task_init(&run, VSS_TASK_SWEEP, &sweep_config, 1, buffer_data);
	vss_task_start(&run);

	vss_task_set_error(&run, msg);

	TEST_ASSERT_EQUAL(VSS_DEVICE_RUN_FINISHED, vss_task_get_state(&run));
}

void test_get_error(void)
{
	const char* msg = "Test error message";

	vss_task_init(&run, VSS_TASK_SWEEP, &sweep_config, 1, buffer_data);
	vss_task_start(&run);

	const char* msg2 = vss_task_get_error(&run);
	TEST_ASSERT_EQUAL(NULL, msg2);

	vss_task_set_error(&run, msg);

	msg2 = vss_task_get_error(&run);

	TEST_ASSERT_EQUAL(msg, msg2);
}

void test_overflow(void)
{
	const power_t v = 0x70fe;

	vss_task_init(&run, VSS_TASK_SWEEP, &sweep_config, -1, buffer_data);
	vss_task_start(&run);

	int n;
	for(n = 0; n < buffer_data_len+100; n++) {
		vss_task_insert_sweep(&run, v, 0xdeadbeef);
	}

	TEST_ASSERT_EQUAL(VSS_DEVICE_RUN_SUSPENDED, vss_task_get_state(&run));
	TEST_ASSERT_FALSE(vss_task_get_error(&run));

	struct vss_task_read_result ctx;
	vss_task_read(&run, &ctx);

	unsigned int channel;
	uint32_t timestamp;
	power_t power;

	while(!vss_task_read_parse(&ctx, &timestamp, &channel, &power));

	TEST_ASSERT_EQUAL(VSS_DEVICE_RUN_RUNNING, vss_task_get_state(&run));
	TEST_ASSERT_EQUAL(1, resume_called);

	n = vss_task_insert_sweep(&run, v, 0xdeadbeef);
	TEST_ASSERT_EQUAL(VSS_OK, n);
}

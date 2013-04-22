#include "unity.h"
#include "run.h"

#define buffer_data_len 1024

static struct vss_device_run run;
static power_t buffer_data[buffer_data_len];

int run_f(void* priv, struct vss_device_run* run)
{
	return VSS_OK;
}

static const struct vss_device device = {
	.run = run_f
};

static const struct vss_device_config device_config = {
	.device = &device
};

static const struct vss_sweep_config sweep_config = {
	.device_config = &device_config,

	.channel_start = 0,
	.channel_stop = 10,
	.channel_step = 1
};

void setUp(void)
{
}

void tearDown(void)
{
}

void test_init(void)
{
	vss_device_run_init(&run, &sweep_config, 1, buffer_data);
	TEST_ASSERT_EQUAL(VSS_DEVICE_RUN_NEW, vss_device_run_get_state(&run));
}

void test_start(void)
{
	vss_device_run_init(&run, &sweep_config, 1, buffer_data);
	vss_device_run_start(&run);
	TEST_ASSERT_EQUAL(VSS_DEVICE_RUN_RUNNING, vss_device_run_get_state(&run));
}

void test_single_run(void)
{
	const power_t v = 0x70fe;

	vss_device_run_init(&run, &sweep_config, 1, buffer_data);
	vss_device_run_start(&run);

	int cnt = 0;
	while(1) {
		int r = vss_device_run_insert(&run, v, 0xdeadbeef);
		if(r == VSS_OK) {
			cnt++;
		} else if(r == VSS_STOP) {
			cnt++;
			break;
		} else {
			break;
		}
	}

	TEST_ASSERT_EQUAL(10, cnt);
}

void test_infinite_run(void)
{
	const power_t v = 0x70fe;

	vss_device_run_init(&run, &sweep_config, -1, buffer_data);
	vss_device_run_start(&run);

	int cnt, r;
	for(cnt = 0; cnt < 100; cnt++) {
		r = vss_device_run_insert(&run, v, 0xdeadbeef);
		TEST_ASSERT_EQUAL(VSS_OK, r);
	}

	vss_device_run_stop(&run);

	for(cnt = 0; cnt < 100; cnt++) {
		r = vss_device_run_insert(&run, v, 0xdeadbeef);
		if(r != VSS_OK) break;
	}

	TEST_ASSERT_EQUAL(VSS_STOP, r);
	TEST_ASSERT_TRUE(cnt <= 10);
	TEST_ASSERT_EQUAL(VSS_DEVICE_RUN_FINISHED, vss_device_run_get_state(&run));
}

void test_read(void)
{
	const power_t v = 0x70fe;

	vss_device_run_init(&run, &sweep_config, -1, buffer_data);
	vss_device_run_start(&run);

	int cnt;
	for(cnt = 0; cnt < 10; cnt++) {
		vss_device_run_insert(&run, v, 0xdeadbeef);
	}

	struct vss_device_run_read_result ctx;
	vss_device_run_read(&run, &ctx);

	int channel;
	uint32_t timestamp;
	power_t power;

	cnt = 0;
	while(vss_device_run_read_parse(&run, &ctx, &timestamp, &channel, &power) == VSS_OK) {
		if(channel != -1) {
			TEST_ASSERT_EQUAL(0xdeadbeef, timestamp);
			TEST_ASSERT_EQUAL(v, power);
			cnt++;
		}
	}

	TEST_ASSERT_EQUAL(10, cnt);
}

void test_get_channel(void)
{
	vss_device_run_init(&run, &sweep_config, 1, buffer_data);
	vss_device_run_start(&run);

	int channel = vss_device_run_get_channel(&run);
	TEST_ASSERT_EQUAL(sweep_config.channel_start, channel);

	vss_device_run_insert(&run, 0, 0);

	channel = vss_device_run_get_channel(&run);
	TEST_ASSERT_EQUAL(sweep_config.channel_start + sweep_config.channel_step, channel);
}

void test_set_error(void)
{
	const char* msg = "Test error message";

	vss_device_run_init(&run, &sweep_config, 1, buffer_data);
	vss_device_run_start(&run);

	vss_device_run_set_error(&run, msg);

	TEST_ASSERT_EQUAL(VSS_DEVICE_RUN_FINISHED, vss_device_run_get_state(&run));
}

void test_get_error(void)
{
	const char* msg = "Test error message";

	vss_device_run_init(&run, &sweep_config, 1, buffer_data);
	vss_device_run_start(&run);

	const char* msg2 = vss_device_run_get_error(&run);
	TEST_ASSERT_EQUAL(NULL, msg2);

	vss_device_run_set_error(&run, msg);

	msg2 = vss_device_run_get_error(&run);

	TEST_ASSERT_EQUAL(msg, msg2);
}

void test_overflow(void)
{
	const power_t v = 0x70fe;

	vss_device_run_init(&run, &sweep_config, -1, buffer_data);
	vss_device_run_start(&run);

	int n;
	for(n = 0; n < buffer_data_len+100; n++) {
		vss_device_run_insert(&run, v, 0xdeadbeef);
	}

	TEST_ASSERT_EQUAL(VSS_DEVICE_RUN_FINISHED, vss_device_run_get_state(&run));
	TEST_ASSERT_TRUE(vss_device_run_get_error(&run));
}

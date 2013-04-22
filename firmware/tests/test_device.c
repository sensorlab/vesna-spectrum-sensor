#include "unity.h"
#include "device.h"

const struct vss_device device_a;

const struct vss_device_config config_a = {
	.device = &device_a
};

const struct vss_device_config config_b = {
	.device = &device_a,
};

void setUp(void)
{
	vss_device_config_add(&config_a);
	vss_device_config_add(&config_b);
}

void tearDown(void)
{
}

void test_config_add(void)
{
	TEST_ASSERT_EQUAL(2, vss_device_config_list_num);
}

void test_config_get(void)
{
	const struct vss_device_config* c;

	c = vss_device_config_get(0, 0);
	TEST_ASSERT_EQUAL(&config_a, c);

	c = vss_device_config_get(0, 1);
	TEST_ASSERT_EQUAL(&config_b, c);
}

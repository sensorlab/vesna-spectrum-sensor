#include "unity.h"
#include "buffer.h"

#define buffer_data_len 1024

static struct vss_buffer buffer;
static uint16_t buffer_data[buffer_data_len];

void setUp(void)
{
	vss_buffer_init(&buffer, buffer_data);
}

void tearDown(void)
{
}

void test_new_is_empty(void)
{
	size_t r = vss_buffer_size(&buffer);
	TEST_ASSERT_EQUAL(0, r);
}

void test_empty_read(void)
{
	const uint16_t *ptr;
	size_t len;

	vss_buffer_read_block(&buffer, &ptr, &len);

	TEST_ASSERT_EQUAL(0, len);
}

void test_read_write(void)
{
	const uint16_t v = 0xc0fe;

	int r = vss_buffer_write(&buffer, v);

	TEST_ASSERT_EQUAL(0, r);

	const uint16_t* ptr;
	size_t len;

	vss_buffer_read_block(&buffer, &ptr, &len);

	TEST_ASSERT_EQUAL(1, len);
	TEST_ASSERT_EQUAL(v, ptr[0]);
}

void test_write_full(void)
{
	const uint16_t v = 0xc0fe;

	size_t n;
	for(n = 0; n < buffer_data_len - 1; n++) {
		int r = vss_buffer_write(&buffer, v);
		TEST_ASSERT_EQUAL(0, r);
	}

	int r = vss_buffer_write(&buffer, v);
	TEST_ASSERT_EQUAL(-1, r);

	size_t s = vss_buffer_size(&buffer);
	TEST_ASSERT_EQUAL(buffer_data_len - 1, s);
}

void prepare_wrap_around(void)
{
	size_t n;

	for(n = 0; n < buffer_data_len/2; n++) {
		vss_buffer_write(&buffer, 0);

		const uint16_t* ptr;
		size_t len;
		vss_buffer_read_block(&buffer, &ptr, &len);
		vss_buffer_release_block(&buffer);
	}
}

void test_write_half(void)
{
	prepare_wrap_around();

	size_t s = vss_buffer_size(&buffer);
	TEST_ASSERT_EQUAL(0, s);

	prepare_wrap_around();

	s = vss_buffer_size(&buffer);
	TEST_ASSERT_EQUAL(0, s);

	prepare_wrap_around();

	s = vss_buffer_size(&buffer);
	TEST_ASSERT_EQUAL(0, s);
}

void test_write_full_wrap_around(void)
{
	prepare_wrap_around();

	const uint16_t v = 0xc0fe;

	size_t n;
	for(n = 0; n < buffer_data_len - 1; n++) {
		int r = vss_buffer_write(&buffer, v);
		TEST_ASSERT_EQUAL(0, r);
	}

	int r = vss_buffer_write(&buffer, v);
	TEST_ASSERT_EQUAL(-1, r);

	size_t s = vss_buffer_size(&buffer);
	TEST_ASSERT_EQUAL(buffer_data_len - 1, s);
}

void test_read_wrap_around(void)
{
	prepare_wrap_around();

	const uint16_t v = 0xc0fe;

	size_t n;
	for(n = 0; n < buffer_data_len - 1; n++) {
		vss_buffer_write(&buffer, v);
	}

	const uint16_t* data;
	size_t len;

	size_t sum = 0;
	do {
		vss_buffer_read_block(&buffer, &data, &len);
		for(n = 0; n < len; n++) {
			TEST_ASSERT_EQUAL(v, data[n]);
		}
		vss_buffer_release_block(&buffer);

		sum += len;
	} while(len != 0);
	
	TEST_ASSERT_EQUAL(buffer_data_len - 1, sum);
}

void test_dont_overwrite_values_just_read(void)
{
	const uint16_t v1 = 0xc0fe;
	const uint16_t v2 = 0xbeef;

	size_t n;
	for(n = 0; n < 100; n++) {
		vss_buffer_write(&buffer, v1);
	}

	const uint16_t* data;
	size_t len;

	vss_buffer_read_block(&buffer, &data, &len);

	while(!vss_buffer_write(&buffer, v2));

	for(n = 0; n < len; n++) {
		TEST_ASSERT_EQUAL(v1, data[n]);
	}

	vss_buffer_release_block(&buffer);

	int r = vss_buffer_write(&buffer, v2);
	
	TEST_ASSERT_EQUAL(0, r);
}

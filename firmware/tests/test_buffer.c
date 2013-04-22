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
#include "unity.h"
#include "buffer.h"

#define buffer_data_len 1024

static struct vss_buffer buffer;
static power_t buffer_data[buffer_data_len];

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
	const power_t *ptr;
	size_t len;

	vss_buffer_read_block(&buffer, &ptr, &len);

	TEST_ASSERT_EQUAL(0, len);
}

void test_read_write(void)
{
	const power_t v = 0x70fe;

	int r = vss_buffer_write(&buffer, v);

	TEST_ASSERT_EQUAL(0, r);

	const power_t* ptr;
	size_t len;

	vss_buffer_read_block(&buffer, &ptr, &len);

	TEST_ASSERT_EQUAL(1, len);
	TEST_ASSERT_EQUAL(v, ptr[0]);
}

void test_write_full(void)
{
	const power_t v = 0x70fe;

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

		const power_t* ptr;
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

	const power_t v = 0x70fe;

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

	const power_t v = 0x70fe;

	size_t n;
	for(n = 0; n < buffer_data_len - 1; n++) {
		vss_buffer_write(&buffer, v);
	}

	const power_t* data;
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
	const power_t v1 = 0x70fe;
	const power_t v2 = 0xbeef;

	size_t n;
	for(n = 0; n < 100; n++) {
		vss_buffer_write(&buffer, v1);
	}

	const power_t* data;
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

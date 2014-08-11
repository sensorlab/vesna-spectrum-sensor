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
#include "buffer.h"

#define buffer_data_len 1024
#define block_size 128

#define nblocks (buffer_data_len/block_size)

static struct vss_buffer buffer;
static power_t buffer_data[buffer_data_len];

void setUp(void)
{
	vss_buffer_init(&buffer, block_size, buffer_data);
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
	power_t *ptr;

	vss_buffer_read(&buffer, &ptr);

	TEST_ASSERT_EQUAL(NULL, ptr);
}

void test_read_write(void)
{
	power_t *wptr;

	vss_buffer_reserve(&buffer, &wptr);
	memset(wptr, 0x42, block_size*sizeof(*wptr));
	vss_buffer_write(&buffer, wptr);
	power_t *rptr;
	vss_buffer_read(&buffer, &rptr);

	int n;
	for(n = 0; n < block_size; n++) {
		TEST_ASSERT_EQUAL(0x4242, rptr[n]);
	}
}

void test_write_full(void)
{
	power_t *wptr;

	size_t n;
	for(n = 0; n < nblocks; n++) {
		vss_buffer_reserve(&buffer, &wptr);
		TEST_ASSERT_TRUE(wptr != NULL);
		vss_buffer_write(&buffer, wptr);
	}

	vss_buffer_reserve(&buffer, &wptr);
	TEST_ASSERT_EQUAL(NULL, wptr);

	size_t s = vss_buffer_size(&buffer);
	TEST_ASSERT_EQUAL(nblocks, s);
}

void read_write_half(void)
{
	size_t n;

	for(n = 0; n < nblocks/2; n++) {
		power_t *wptr;
		vss_buffer_reserve(&buffer, &wptr);
		vss_buffer_write(&buffer, wptr);

		power_t* rptr;
		vss_buffer_read(&buffer, &rptr);
		vss_buffer_release(&buffer, rptr);
	}
}

void test_write_half(void)
{
	read_write_half();

	size_t s = vss_buffer_size(&buffer);
	TEST_ASSERT_EQUAL(0, s);

	read_write_half();

	s = vss_buffer_size(&buffer);
	TEST_ASSERT_EQUAL(0, s);

	read_write_half();

	s = vss_buffer_size(&buffer);
	TEST_ASSERT_EQUAL(0, s);
}

void test_write_full_wrap_around(void)
{
	read_write_half();

	power_t *wptr;
	size_t n;
	for(n = 0; n < nblocks; n++) {
		vss_buffer_reserve(&buffer, &wptr);
		TEST_ASSERT_TRUE(wptr != NULL);
		vss_buffer_write(&buffer, wptr);
	}

	vss_buffer_reserve(&buffer, &wptr);
	TEST_ASSERT_EQUAL(NULL, wptr);

	size_t s = vss_buffer_size(&buffer);
	TEST_ASSERT_EQUAL(nblocks, s);
}

void test_read_wrap_around(void)
{
	read_write_half();

	size_t n;
	for(n = 0; n < nblocks; n++) {
		power_t *wptr;
		vss_buffer_reserve(&buffer, &wptr);
		TEST_ASSERT_TRUE(wptr != NULL);
		memset(wptr, 0x42, block_size*sizeof(*wptr));
		vss_buffer_write(&buffer, wptr);
	}

	power_t* rptr;

	size_t sum = 0;
	while(1) {
		vss_buffer_read(&buffer, &rptr);
		if(rptr == NULL) break;

		for(n = 0; n < block_size; n++) {
			TEST_ASSERT_EQUAL(0x4242, rptr[n]);
		}
		vss_buffer_release(&buffer, rptr);

		sum++;
	}
	
	TEST_ASSERT_EQUAL(nblocks, sum);
}

void test_dont_overwrite_values_just_read(void)
{
	power_t* wptr;

	size_t n;
	for(n = 0; n < nblocks/2; n++) {
		vss_buffer_reserve(&buffer, &wptr);
		memset(wptr, 0x01, block_size*sizeof(*wptr));
		vss_buffer_write(&buffer, wptr);
	}

	power_t* rptr;

	vss_buffer_read(&buffer, &rptr);

	while(1) {
		power_t* wptr;
		vss_buffer_reserve(&buffer, &wptr);
		if(wptr == NULL) break;

		memset(wptr, 0x02, block_size);
		vss_buffer_write(&buffer, wptr);
	}

	for(n = 0; n < block_size; n++) {
		TEST_ASSERT_EQUAL_HEX(0x0101, rptr[n]);
	}

	vss_buffer_release(&buffer, rptr);

	vss_buffer_reserve(&buffer, &wptr);
	TEST_ASSERT_TRUE(wptr != NULL);
}

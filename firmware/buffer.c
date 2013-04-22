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
#include "buffer.h"
#include "vss.h"

/** @brief Initialize a circular buffer.
 *
 * @param buffer Pointer to the circular buffer to initialize.
 * @param data Pointer to the allocated storage array.
 * @param data_len Length of the allocated storage array.
 */
void vss_buffer_init_size(struct vss_buffer* buffer, power_t* data, size_t data_len)
{
	buffer->start = data;
	buffer->end = data + data_len;

	buffer->released = data;
	buffer->write = data;
}

/** @brief Get number of measurements currently stored in the buffer.
 *
 * @param buffer Pointer to the circular buffer.
 * @return Number of measurements currently stored in the buffer.
 */
size_t vss_buffer_size(const struct vss_buffer* buffer)
{
	power_t *write = buffer->write;
	if(buffer->released <= write) {
		return write - buffer->released;
	} else {
		return (buffer->end - buffer->released) + (write - buffer->start);
	}
}

/** @brief Read a block of measurements from the buffer.
 *
 * If buffer is currently empty, function will return a block of length 0.
 *
 * After the caller is done with measurements pointed to by @a data, it should
 * call vss_buffer_release_block().
 *
 * @param buffer Pointer to the circular buffer.
 * @param data Pointer to a block of measurements for reading.
 * @param data_len Number of measurements read.
 */
void vss_buffer_read_block(struct vss_buffer* buffer, const power_t** data, size_t* data_len)
{
	*data = buffer->released;
	power_t *write = buffer->write;

	if(buffer->released <= write) {
		*data_len = write - buffer->released;
		buffer->read = write;
	} else {
		*data_len = buffer->end - buffer->released;
		buffer->read = buffer->start;
	}
}

/** @brief Release a block of measurements after reading.
 *
 * Should be called once after each call to vss_buffer_read_block().
 *
 * @param buffer Pointer to the circular buffer.
 */
void vss_buffer_release_block(struct vss_buffer* buffer)
{
	buffer->released = buffer->read;
}

/** @brief Write a single measurement to the circular buffer.
 *
 * @param buffer Pointer to the circular buffer.
 * @param data Measurement to write to the buffer.
 * @return VSS_ERROR if buffer is full or VSS_OK otherwise.
 */
int vss_buffer_write(struct vss_buffer* buffer, power_t data)
{
	power_t* write = buffer->write;

	write++;
	if(write == buffer->end) {
		write = buffer->start;
	}

	if(write == buffer->released) {
		return VSS_ERROR;
	} else {
		*(buffer->write) = data;
		buffer->write = write;
		return VSS_OK;
	}
}

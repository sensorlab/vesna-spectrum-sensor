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
void vss_buffer_init_size(struct vss_buffer* buffer, size_t block_size,
		power_t* data, size_t data_len)
{
	buffer->block_size = block_size;

	buffer->start = data;
	buffer->end = data + data_len;

	buffer->read = data;
	buffer->write = data;

	buffer->n_read = 0;
	buffer->n_write = 0;
}

/** @brief Get number of blocks currently stored in the buffer.
 *
 * @param buffer Pointer to the circular buffer.
 * @return Number of blocks currently stored in the buffer.
 */
size_t vss_buffer_size(const struct vss_buffer* buffer)
{
	return buffer->n_write - buffer->n_read;
}

/** @brief Read a block from the buffer.
 *
 * If buffer is currently empty, data is set to NULL.
 *
 * After the caller is done with measurements pointed to by @a data, it should
 * call vss_buffer_release().
 *
 * @param buffer Pointer to the circular buffer.
 * @param data Pointer to a block for reading.
 */
void vss_buffer_read(struct vss_buffer* buffer, power_t** data)
{
	if(buffer->n_write > buffer->n_read) {
		*data = buffer->read;
	} else {
		*data = NULL;
	}
}

/** @brief Release a block of measurements after reading.
 *
 * Should be called once after each call to vss_buffer_read().
 *
 * @param buffer Pointer to the circular buffer.
 * @param data Pointer to the block obtained from vss_buffer_read().
 */
void vss_buffer_release(struct vss_buffer* buffer, power_t* data)
{
	buffer->read = data + buffer->block_size;
	if(buffer->read >= buffer->end) {
		buffer->read = buffer->start;
	}
	buffer->n_read++;
}

/** @brief Reserve a block for writing to the circular buffer.
 *
 * If buffer is full, data is set to NULL.
 *
 * @param buffer Pointer to the circular buffer.
 * @param data Pointer to the block for writing.
 */
void vss_buffer_reserve(struct vss_buffer* buffer, power_t** data)
{
	if(buffer->read != buffer->write || buffer->n_write == buffer->n_read) {
		*data = buffer->write;
	} else {
		*data = NULL;
	}
}

/** @brief Write a reserved block to the circular buffer.
 *
 * @param buffer Pointer to the circular buffer.
 * @param data Pointer to the block obtained from vss_buffer_read().
 */
void vss_buffer_write(struct vss_buffer* buffer, power_t* data)
{
	buffer->write = data + buffer->block_size;
	if(buffer->write >= buffer->end) {
		buffer->write = buffer->start;
	}
	buffer->n_write++;
}

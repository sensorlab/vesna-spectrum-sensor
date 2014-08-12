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
/** @file
 * @brief Simple circular buffer implementation for power measurements.
 *
 * Writes are by single elements, reads are by blocks. Implementation is
 * zero-copy: when reading a block of values, a pointer directly into the
 * buffer storage is given. Once the reader has finished processing the data,
 * the block must be released so that storage can be reused by the writer.
 *
 * Writer is typically an interrupt routing, reader is main thread. */

#ifndef HAVE_BUFFER_H
#define HAVE_BUFFER_H

#include <stddef.h>
#include <stdint.h>

/** @brief Type used for power measurements. */
typedef int16_t power_t;

/** @brief Circular buffer. */
struct vss_buffer {
	/** @brief Size of blocks */
	size_t block_size;

	/** @brief Pointer to the first element of allocated buffer. */
	void* start;
	/** @brief Pointer to the one after the last element in the allocated buffer. */
	void* end;

	/** @brief Pointer to the one after the furthest position already read. */
	void* read;
	/** @brief Pointer to the last written position. */
	void* volatile write;

	/** @brief Total number of blocks read from the buffer. */
	int n_read;

	/** @brief Total number of blocks written to the buffer. */
	int n_write;
};

/** @brief Initialize a circular buffer with statically allocated storage.
 *
 * Typical usage:
 *
 * @code{.c}
 * power_t data[512];
 * struct vss_buffer buffer;
 *
 * vss_buffer_init(&buffer, data);
 * @endcode
 *
 * @param buffer Pointer to the circular buffer to initialize.
 * @param data Array to use as buffer storage.
 */
#define vss_buffer_init(buffer, block_size, data) \
	vss_buffer_init_size(buffer, block_size, data, sizeof(data))

void vss_buffer_init_size(struct vss_buffer* buffer, size_t block_size,
		void* data, size_t data_len);
size_t vss_buffer_size(const struct vss_buffer* buffer);
size_t vss_buffer_size2(const struct vss_buffer* buffer);

void vss_buffer_read(struct vss_buffer* buffer, void** data);
void vss_buffer_release(struct vss_buffer* buffer, void* data);
void vss_buffer_reserve(struct vss_buffer* buffer, void** data);
void vss_buffer_write(struct vss_buffer* buffer, void* data);

#endif

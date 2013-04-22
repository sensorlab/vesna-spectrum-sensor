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
	/** @brief Pointer to the first element in the allocated buffer. */
	power_t* start;
	/** @brief Pointer to the one after the last element in the allocated buffer. */
	power_t* end;

	/** @brief Pointer to the one after the furthest position already read. */
	power_t* read;
	/** @brief Pointer to the last written position. */
	power_t* volatile write;
	/** @brief Pointer to the one after the last free element. */
	power_t* released;
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
#define vss_buffer_init(buffer, data) vss_buffer_init_size(buffer, data, sizeof(data)/sizeof(*data))

void vss_buffer_init_size(struct vss_buffer* buffer, power_t* data, size_t data_len);
size_t vss_buffer_size(const struct vss_buffer* buffer);

void vss_buffer_read_block(struct vss_buffer* buffer, const power_t** data, size_t* data_len);
void vss_buffer_release_block(struct vss_buffer* buffer);
int vss_buffer_write(struct vss_buffer* buffer, power_t data);

#endif

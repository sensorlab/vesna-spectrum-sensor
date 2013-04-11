#ifndef HAVE_BUFFER_H
#define HAVE_BUFFER_H

#include <stddef.h>
#include <stdint.h>

struct vss_buffer {
	uint16_t* start;
	uint16_t* end;

	uint16_t* read;
	uint16_t* volatile write;
	uint16_t* released;
};

#define vss_buffer_init(buffer, data) vss_buffer_init_size(buffer, data, sizeof(data)/sizeof(*data))

void vss_buffer_init_size(struct vss_buffer* buffer, uint16_t* data, size_t data_len);
size_t vss_buffer_size(const struct vss_buffer* buffer);

void vss_buffer_read_block(struct vss_buffer* buffer, const uint16_t** data, size_t* data_len);
void vss_buffer_release_block(struct vss_buffer* buffer);
int vss_buffer_write(struct vss_buffer* buffer, uint16_t data);

#endif

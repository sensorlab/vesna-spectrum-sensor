#include "buffer.h"

void vss_buffer_init_size(struct vss_buffer* buffer, uint16_t* data, size_t data_len)
{
	buffer->start = data;
	buffer->end = data + data_len;

	buffer->released = data;
	buffer->write = data;
}

size_t vss_buffer_size(const struct vss_buffer* buffer)
{
	if(buffer->released <= buffer->write) {
		return buffer->write - buffer->released;
	} else {
		return (buffer->end - buffer->released) + (buffer->write - buffer->start);
	}
}

void vss_buffer_read_block(struct vss_buffer* buffer, const uint16_t** data, size_t* data_len)
{
	*data = buffer->released;

	if(buffer->released <= buffer->write) {
		*data_len = buffer->write - buffer->released;
		buffer->read = buffer->write;
	} else {
		*data_len = buffer->end - buffer->released;
		buffer->read = buffer->start;
	}
}

void vss_buffer_release_block(struct vss_buffer* buffer)
{
	buffer->released = buffer->read;
}

int vss_buffer_write(struct vss_buffer* buffer, uint16_t data)
{
	uint16_t* write = buffer->write;

	write++;
	if(write == buffer->end) {
		write = buffer->start;
	}

	if(write == buffer->released) {
		return -1;
	} else {
		*(buffer->write) = data;
		buffer->write = write;
		return 0;
	}
}

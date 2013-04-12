#include "buffer.h"

void vss_buffer_init_size(struct vss_buffer* buffer, power_t* data, size_t data_len)
{
	buffer->start = data;
	buffer->end = data + data_len;

	buffer->released = data;
	buffer->write = data;
}

size_t vss_buffer_size(const struct vss_buffer* buffer)
{
	power_t *write = buffer->write;
	if(buffer->released <= write) {
		return write - buffer->released;
	} else {
		return (buffer->end - buffer->released) + (write - buffer->start);
	}
}

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

void vss_buffer_release_block(struct vss_buffer* buffer)
{
	buffer->released = buffer->read;
}

int vss_buffer_write(struct vss_buffer* buffer, power_t data)
{
	power_t* write = buffer->write;

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

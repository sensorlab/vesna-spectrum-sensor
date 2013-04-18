#ifndef HAVE_RUN_H
#define HAVE_RUN_H

#include "vss.h"
#include "device.h"
#include "buffer.h"

enum vss_device_run_state {
	VSS_DEVICE_RUN_NEW,
	VSS_DEVICE_RUN_RUNNING,
	VSS_DEVICE_RUN_FINISHED
};

struct vss_device_run {
	struct vss_buffer buffer;

	const struct vss_sweep_config* sweep_config;

	enum vss_device_run_state state;

	volatile int sweep_num;

	unsigned int write_channel;

	unsigned int read_channel;
	int read_state;

	volatile int running;

	const char* volatile error_msg;
};

struct vss_device_run_read_result {
	const power_t* data;
	size_t len;

	size_t p;
};

#define vss_device_run_init(device_run, sweep_config, sweep_num, data) {\
	vss_buffer_init(&(device_run)->buffer, data); \
	vss_device_run_init_(device_run, sweep_config, sweep_num); \
}

void vss_device_run_init_(struct vss_device_run* device_run, const struct vss_sweep_config* sweep_config, 
		int sweep_num);

unsigned int vss_device_run_get_channel(struct vss_device_run* device_run);
int vss_device_run_insert(struct vss_device_run* device_run, power_t data, uint32_t timestamp);

int vss_device_run_start(struct vss_device_run* run);
int vss_device_run_stop(struct vss_device_run* run);
enum vss_device_run_state vss_device_run_get_state(struct vss_device_run* run);
void vss_device_run_set_error(struct vss_device_run* run, const char* msg);

void vss_device_run_read(struct vss_device_run* run, struct vss_device_run_read_result* ctx);
int vss_device_run_read_parse(struct vss_device_run* run, struct vss_device_run_read_result *ctx,
		uint32_t* timestamp, int* channel, power_t* power);
const char* vss_device_run_get_error(struct vss_device_run* run);

#endif

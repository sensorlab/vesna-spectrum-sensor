#ifndef HAVE_RUN_H
#define HAVE_RUN_H

#include "vss.h"
#include "device.h"
#include "buffer.h"

struct vss_device_run {
	struct vss_buffer buffer;

	const struct vss_sweep_config* sweep_config;

	volatile unsigned int sweep_num;

	volatile unsigned int overflow_num;

	unsigned int channel;

	volatile int running;
};

#define vss_device_run_init(device_run, sweep_config, sweep_num, data) {\
	vss_buffer_init(&(device_run)->buffer, data); \
	vss_device_run_init_(device_run, sweep_config, sweep_num); \
}

void vss_device_run_init_(struct vss_device_run* device_run, const struct vss_sweep_config* sweep_config, 
		unsigned int sweep_num);
int vss_device_run_insert(struct vss_device_run* device_run, uint16_t data, uint32_t timestamp);

int vss_device_run_start(struct vss_device_run* run);
int vss_device_run_stop(struct vss_device_run* run);
int vss_device_run_is_running(struct vss_device_run* run);

#endif

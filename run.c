#include "run.h"

void vss_device_run_init_(struct vss_device_run* device_run, const struct vss_sweep_config* sweep_config, 
		unsigned int sweep_num)
{
	device_run->sweep_config = sweep_config;
	device_run->sweep_num = sweep_num;

	device_run->overflow_num = 0;
	device_run->channel = sweep_config->channel_start;
	device_run->running = 0;
}

static void vss_device_run_write(struct vss_device_run* device_run, uint16_t data)
{
	if(vss_buffer_write(&device_run->buffer, data)) {
		device_run->overflow_num++;
	}
}

static void vss_device_run_insert_timestamp(struct vss_device_run* device_run, uint32_t timestamp)
{
	vss_device_run_write(device_run, (timestamp >>  0) & 0x0000ffff);
	vss_device_run_write(device_run, (timestamp >> 16) & 0x0000ffff);
}

int vss_device_run_insert(struct vss_device_run* device_run, uint16_t data, uint32_t timestamp)
{
	if(device_run->channel == device_run->sweep_config->channel_start) {
		vss_device_run_insert_timestamp(device_run, timestamp);
	}

	vss_device_run_write(device_run, data);

	device_run->channel += device_run->sweep_config->channel_step;
	if(device_run->channel >= device_run->sweep_config->channel_stop) {
		if(device_run->sweep_num > 1) {
			device_run->sweep_num--;
			device_run->channel = device_run->sweep_config->channel_start;

			return VSS_OK;
		} else {
			device_run->running = 0;

			return VSS_STOP;
		}
	} else {
		return VSS_OK;
	}
}

int vss_device_run_start(struct vss_device_run* run)
{
	run->running = 1;

	const struct vss_device* device = run->sweep_config->device_config->device;

	int r = device->run(device->priv, run);
	if(r != VSS_OK) {
		run->running = 0;
	}

	return r;
}

int vss_device_run_stop(struct vss_device_run* run)
{
	run->sweep_num = 0;
	return VSS_OK;
}

int vss_device_run_is_running(struct vss_device_run* run)
{
	return run->running;
}

#include "run.h"

void vss_device_run_init_(struct vss_device_run* device_run, const struct vss_sweep_config* sweep_config, 
		int sweep_num)
{
	device_run->sweep_config = sweep_config;
	device_run->sweep_num = sweep_num;

	device_run->state = VSS_DEVICE_RUN_NEW;
	device_run->write_channel = sweep_config->channel_start;
	device_run->error_msg = NULL;

	device_run->read_state = 0;
	device_run->read_channel = sweep_config->channel_start;
}

static int vss_device_run_write(struct vss_device_run* device_run, power_t data)
{
	if(vss_buffer_write(&device_run->buffer, data)) {
		vss_device_run_set_error(device_run, "buffer overflow");
		return VSS_ERROR;
	} else {
		return VSS_OK;
	}
}

static int vss_device_run_insert_timestamp(struct vss_device_run* device_run, uint32_t timestamp)
{
	if(vss_device_run_write(device_run, (timestamp >>  0) & 0x0000ffff)) {
		return VSS_ERROR;
	}
	if(vss_device_run_write(device_run, (timestamp >> 16) & 0x0000ffff)) {
		return VSS_ERROR;
	}

	return VSS_OK;
}

unsigned int vss_device_run_get_channel(struct vss_device_run* device_run)
{
	return device_run->write_channel;
}

int vss_device_run_insert(struct vss_device_run* device_run, power_t data, uint32_t timestamp)
{
	if(device_run->write_channel == device_run->sweep_config->channel_start) {
		if(vss_device_run_insert_timestamp(device_run, timestamp)) {
			return VSS_STOP;
		}
	}

	if(vss_device_run_write(device_run, data)) {
		return VSS_STOP;
	}

	device_run->write_channel += device_run->sweep_config->channel_step;
	if(device_run->write_channel >= device_run->sweep_config->channel_stop) {
		if(device_run->sweep_num > 1 || device_run->sweep_num < 0) {
			device_run->sweep_num--;
			device_run->write_channel = device_run->sweep_config->channel_start;

			return VSS_OK;
		} else {
			device_run->state = VSS_DEVICE_RUN_FINISHED;
			return VSS_STOP;
		}
	} else {
		return VSS_OK;
	}
}

void vss_device_run_set_error(struct vss_device_run* run, const char* msg)
{
	run->error_msg = msg;
	run->state = VSS_DEVICE_RUN_FINISHED;
}

const char* vss_device_run_get_error(struct vss_device_run* run)
{
	return run->error_msg;
}

int vss_device_run_start(struct vss_device_run* run)
{
	run->state = VSS_DEVICE_RUN_RUNNING;

	const struct vss_device* device = run->sweep_config->device_config->device;

	int r = vss_device_run(device, run);
	if(r != VSS_OK) {
		run->state = VSS_DEVICE_RUN_FINISHED;
	}

	return r;
}

int vss_device_run_stop(struct vss_device_run* run)
{
	run->sweep_num = 0;
	return VSS_OK;
}

enum vss_device_run_state vss_device_run_get_state(struct vss_device_run* run)
{
	return run->state;
}

void vss_device_run_read(struct vss_device_run* run, struct vss_device_run_read_result* ctx)
{
	ctx->p = 0;
	vss_buffer_read_block(&run->buffer, &ctx->data, &ctx->len);
}

int vss_device_run_read_parse(struct vss_device_run* run, struct vss_device_run_read_result *ctx,
		uint32_t* timestamp, int* channel, power_t* power)
{
	if(ctx->p >= ctx->len) {
		vss_buffer_release_block(&run->buffer);
		return VSS_STOP;
	}

	switch(run->read_state) {
		case 0:
			*timestamp = (uint16_t) ctx->data[ctx->p];
			*channel = -1;

			run->read_state = 1;
			break;
		case 1:
			*timestamp |= ctx->data[ctx->p] << 16;
			*channel = -1;

			run->read_state = 2;
			break;

		case 2:
			*power = ctx->data[ctx->p];
			*channel = run->read_channel;

			run->read_channel += run->sweep_config->channel_step;
			if(run->read_channel >= run->sweep_config->channel_stop) {
				run->read_channel = run->sweep_config->channel_start;
				run->read_state = 0;
			}
			break;
	}

	ctx->p++;
	return VSS_OK;
}

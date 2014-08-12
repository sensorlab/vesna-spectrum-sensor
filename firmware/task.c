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
#include <assert.h>

#include "task.h"

/** @brief Initialize a device task.
 *
 * @param task Pointer to the task to initialize.
 * @param sweep_config Pointer to the spectrum sweep configuration to use.
 * @param sweep_num Number of spectrum sensing sweeps to perform (use -1 for
 * infinite).
 */
int vss_task_init_size(struct vss_task* task, enum vss_task_type type,
		const struct vss_sweep_config* sweep_config,
		int sweep_num, power_t *data, size_t data_len)
{
	task->type = type;

	task->sweep_config = sweep_config;
	task->sweep_num = sweep_num;

	unsigned sample_num;
	switch(type) {
		case VSS_TASK_SWEEP:
			sample_num = vss_sweep_config_channel_num(sweep_config);
			break;
		case VSS_TASK_SAMPLE:
			sample_num = sweep_config->n_average;
			break;
		default:
			assert(0);
	}

	int r = vss_buffer_init_size(&task->buffer,
			sizeof(*data) * (sample_num+2),
			data, data_len);
	if(r) {
		return r;
	}

	task->state = VSS_DEVICE_RUN_NEW;
	task->write_channel = sweep_config->channel_start;
	task->error_msg = NULL;

	return VSS_OK;
}

static void vss_task_insert_timestamp(struct vss_task* task, uint32_t timestamp)
{
	task->write_ptr[0] = (timestamp >>  0) & 0x0000ffff;
	task->write_ptr[1] = (timestamp >> 16) & 0x0000ffff;
	task->write_ptr += 2;
}

/** @brief Get current channel to measure.
 *
 * Called by the device driver to get the frequency channel of the next measurement.
 *
 * @param task Pointer to the task.
 * @return Channel number for the next measurement.
 */
unsigned int vss_task_get_channel(struct vss_task* task)
{
	return task->write_channel;
}

/** @brief Get number of samples to average.
 *
 * Called by the device driver to get the number of samples to get from the device.
 *
 * @param task Pointer to the task.
 */
unsigned int vss_task_get_n_average(struct vss_task* task)
{
	return task->sweep_config->n_average;
}

static int vss_task_reserve_block_(struct vss_task* task, uint32_t timestamp)
{
	vss_buffer_reserve(&task->buffer, (void**)&task->write_ptr);
	if(task->write_ptr == NULL) {
		vss_task_set_error(task, "buffer overflow");
		return VSS_ERROR;
	}
	vss_task_insert_timestamp(task, timestamp);
	return VSS_OK;
}

/** @brief Add a new measurement result for the task.
 *
 * Called by the device driver to report a new measurement.
 *
 * @param task Pointer to the task.
 * @param data Measurement result.
 * @param timestamp Time of the measurement.
 * @return VSS_STOP if the driver should terminate the task or VSS_OK otherwise.
 */
int vss_task_insert(struct vss_task* task, power_t data, uint32_t timestamp)
{
	if(task->write_channel == task->sweep_config->channel_start) {
		int r = vss_task_reserve_block_(task, timestamp);
		if(r) {
			return r;
		}
	}

	assert((void*) task->write_ptr <
			task->buffer.write + task->buffer.block_size);

	*task->write_ptr = data;
	task->write_ptr++;

	task->write_channel += task->sweep_config->channel_step;
	if(task->write_channel >= task->sweep_config->channel_stop) {
		return vss_task_write_block(task);
	} else {
		return VSS_OK;
	}
}

int vss_task_reserve_block(struct vss_task* task, power_t** data,
		uint32_t timestamp)
{
	int r = vss_task_reserve_block_(task, timestamp);
	if(r) {
		return r;
	}

	*data = task->write_ptr;
	return VSS_OK;
}

int vss_task_write_block(struct vss_task* task)
{
	vss_buffer_write(&task->buffer);
	if(task->sweep_num > 1 || task->sweep_num < 0) {
		task->sweep_num--;
		task->write_channel = task->sweep_config->channel_start;

		return VSS_OK;
	} else {
		task->state = VSS_DEVICE_RUN_FINISHED;
		return VSS_STOP;
	}
}

/** @brief Stop a task with an error.
 *
 * Called by the device driver in case an error was encountered and the task cannot continue.
 *
 * @param task Pointer to the task.
 * @param msg Pointer to the error message.
 */
void vss_task_set_error(struct vss_task* task, const char* msg)
{
	task->error_msg = msg;
	task->state = VSS_DEVICE_RUN_FINISHED;
}

/** @brief Retrieve an error message for a task.
 *
 * Called by the user to retrieve an error message.
 *
 * @param task Pointer to the task.
 * return Pointer to the error message or NULL if task did not encounter an error.
 */
const char* vss_task_get_error(struct vss_task* task)
{
	return task->error_msg;
}

/** @brief Start a task.
 *
 * Called by the user to start a task.
 *
 * @param task Pointer to the task that has been started.
 * @return VSS_OK on success or an error code otherwise.
 */
int vss_task_start(struct vss_task* task)
{
	task->state = VSS_DEVICE_RUN_RUNNING;

	const struct vss_device* device = task->sweep_config->device_config->device;

	int r;
	switch(task->type) {
		case VSS_TASK_SWEEP:
			r = vss_device_run_sweep(device, task);
			break;
		case VSS_TASK_SAMPLE:
			r = vss_device_run_sample(device, task);
			break;
		default:
			assert(0);
	}

	if(r != VSS_OK) {
		task->state = VSS_DEVICE_RUN_FINISHED;
	}

	return r;
}

/** @brief Stop a task.
 *
 * Called by the user to force a task to stop.
 *
 * @param task Pointer to the task to stop.
 * @return VSS_OK on success or an error code otherwise.
 */
int vss_task_stop(struct vss_task* task)
{
	task->sweep_num = 0;
	return VSS_OK;
}

/** @brief Query task state.
 *
 * @param task Pointer to the task to query.
 * @return Current task state.
 */
enum vss_task_state vss_task_get_state(struct vss_task* task)
{
	return task->state;
}

/** @brief Read from the task's circular buffer.
 *
 * @param task Pointer to the task.
 * @param ctx Pointer to the result of the read operation.
 */
void vss_task_read(struct vss_task* task, struct vss_task_read_result* ctx)
{
	vss_buffer_read(&task->buffer, (void**) &ctx->read_ptr);
	ctx->read_channel = task->sweep_config->channel_start;
}

/** @brief Parse the values from the task's circular buffer.
 *
 * @param task Pointer to the task.
 * @param ctx Pointer to the result of the read operation.
 * @param timestamp Timestamp of the measurement.
 * @param channel Channel of the measurement (set to -1 if no measurement yet)
 * @param power Result of the power measurement.
 * @return VSS_STOP if there is nothing more to parse or VSS_OK otherwise.
 */
int vss_task_read_parse(struct vss_task* task, struct vss_task_read_result *ctx,
		uint32_t* timestamp, int* channel, power_t* power)
{
	if(ctx->read_ptr == NULL) {
		return VSS_STOP;
	}

	if(ctx->read_channel == task->sweep_config->channel_start) {
		*timestamp = (uint16_t) ctx->read_ptr[0] | \
			     (ctx->read_ptr[1] << 16);
		ctx->read_ptr += 2;
	}

	if(ctx->read_channel >= task->sweep_config->channel_stop) {
		vss_buffer_release(&task->buffer);
		return VSS_STOP;
	}

	assert((void*)ctx->read_ptr <
			task->buffer.read + task->buffer.block_size);

	*power = *ctx->read_ptr;
	*channel = ctx->read_channel;

	ctx->read_ptr++;
	ctx->read_channel += task->sweep_config->channel_step;

	return VSS_OK;
}

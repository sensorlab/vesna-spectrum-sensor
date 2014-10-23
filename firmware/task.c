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
			sizeof(struct vss_task_block) + sizeof(*data) * sample_num,
			data, data_len);
	if(r) {
		return r;
	}

	task->sample_num = sample_num;

	task->state = VSS_DEVICE_RUN_NEW;
	task->write_channel = sweep_config->channel_start;
	task->error_msg = NULL;

	task->overflows = 0;

	return VSS_OK;
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

static int vss_task_reserve_block(struct vss_task* task, uint32_t timestamp, unsigned int channel)
{
	struct vss_task_block* block;

	vss_buffer_reserve(&task->buffer, (void**)&block);
	if(block == NULL) {
		task->overflows++;
		task->state = VSS_DEVICE_RUN_SUSPENDED;
		return VSS_SUSPEND;
	}

	block->timestamp = timestamp;
	block->channel = channel;

	task->write_ptr = block->data;

	return VSS_OK;
}

static void vss_task_write_block(struct vss_task* task)
{
	vss_buffer_write(&task->buffer);
}

static int vss_task_inc_channel(struct vss_task* task)
{
	task->write_channel += task->sweep_config->channel_step;
	if(task->write_channel >= task->sweep_config->channel_stop) {
		task->write_channel = task->sweep_config->channel_start;

		if(task->sweep_num > 1) {
			task->sweep_num--;
			return VSS_OK;
		} else if(task->sweep_num < 0) {
			return VSS_OK;
		} else {
			task->state = VSS_DEVICE_RUN_FINISHED;
			return VSS_STOP;
		}
	} else {
		return VSS_OK;
	}
}

/** @brief Add a new measurement result for a sweep task.
 *
 * Called by the device driver to report a new measurement.
 *
 * @param task Pointer to the task.
 * @param data Measurement result.
 * @param timestamp Time of the measurement.
 * @return VSS_STOP if the driver should terminate the task or VSS_OK otherwise.
 */
int vss_task_insert_sweep(struct vss_task* task, power_t data, uint32_t timestamp)
{
	if(task->write_channel == task->sweep_config->channel_start) {
		int r = vss_task_reserve_block(task, timestamp, task->write_channel);
		if(r) {
			return r;
		}
	}

	assert((void*) task->write_ptr <
			task->buffer.write + task->buffer.block_size);

	*task->write_ptr = data;
	task->write_ptr++;

	int r = vss_task_inc_channel(task);
	if(task->write_channel == task->sweep_config->channel_start) {
		vss_task_write_block(task);
	}
	return r;
}

/** @brief Reserve a block for sample data.
 *
 * Called by the device driver before starting a new measurement.
 *
 * @param task Pointer to the task.
 * @param data Pointer to the reserved block.
 * @param timestamp Time of the measurement.
 * @return VSS_OK on success or error otherwise.
 */
int vss_task_reserve_sample(struct vss_task* task, power_t** data, uint32_t timestamp)
{
	int r = vss_task_reserve_block(task, timestamp, task->write_channel);
	if(r) {
		return r;
	}

	*data = task->write_ptr;
	return VSS_OK;
}

/** @brief Write the reserved block of sample data.
 *
 * Called by the device driver after concluding a measurement.
 *
 * @param task Pointer to the task.
 * @return VSS_STOP if the driver should terminate the task or VSS_OK otherwise.
 */
int vss_task_write_sample(struct vss_task* task)
{
	vss_task_write_block(task);
	return vss_task_inc_channel(task);
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
			if(task->sweep_config->channel_step <= 0) {
				r = VSS_ERROR;
			} else {
				r = vss_device_run_sweep(device, task);
			}
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
int vss_task_read(struct vss_task* task, struct vss_task_read_result* ctx)
{
	ctx->task = task;
	vss_buffer_read(&task->buffer, (void**) &ctx->block);

	if(ctx->block == NULL) {
		return VSS_ERROR;
	} else {
		ctx->read_ptr = ctx->block->data;
		ctx->read_channel = ctx->block->channel;
		ctx->read_cnt = 0;
		return VSS_OK;
	}
}

/** @brief Parse the values from the task's circular buffer.
 *
 * @param ctx Pointer to the result of the read operation.
 * @param timestamp Timestamp of the measurement.
 * @param channel Channel of the measurement.
 * @param power Result of the power measurement.
 * @return VSS_STOP if there is nothing more to parse or VSS_OK otherwise.
 */
int vss_task_read_parse(struct vss_task_read_result *ctx,
		uint32_t* timestamp, unsigned int* channel, power_t* power)
{
	struct vss_task* const task = ctx->task;

	assert(ctx->block != NULL);

	*timestamp = ctx->block->timestamp;

	if(ctx->read_cnt == task->sample_num) {
		vss_buffer_release(&task->buffer);
		if(task->state == VSS_DEVICE_RUN_SUSPENDED) {
			task->state = VSS_DEVICE_RUN_RUNNING;
			int r = vss_device_resume(
				task->sweep_config->device_config->device,
				task);
			if(r) {
				vss_task_set_error(task, "device resume failed "
						"after buffer overflow");
			}
		}
		return VSS_STOP;
	}

	assert((void*)ctx->read_ptr <
			task->buffer.read + task->buffer.block_size);

	*power = *ctx->read_ptr;
	*channel = ctx->read_channel;

	ctx->read_ptr++;
	ctx->read_cnt++;

	if(task->type == VSS_TASK_SWEEP) {
		ctx->read_channel += task->sweep_config->channel_step;
	}

	return VSS_OK;
}

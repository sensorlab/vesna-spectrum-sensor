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
#include "task.h"

/** @brief Initialize a device task.
 *
 * Note: circular buffer needs to be initialized separately. Use the
 * vss_task_init() macro which does that automatically.
 *
 * @param task Pointer to the task to initialize.
 * @param sweep_config Pointer to the spectrum sweep configuration to use.
 * @param sweep_num Number of spectrum sensing sweeps to perform (use -1 for
 * infinite).
 */
void vss_task_init_(struct vss_task* task, const struct vss_sweep_config* sweep_config,
		int sweep_num)
{
	task->sweep_config = sweep_config;
	task->sweep_num = sweep_num;

	task->state = VSS_DEVICE_RUN_NEW;
	task->write_channel = sweep_config->channel_start;
	task->error_msg = NULL;

	task->read_state = 0;
	task->read_channel = sweep_config->channel_start;
}

static int vss_task_write(struct vss_task* task, power_t data)
{
	if(vss_buffer_write(&task->buffer, data)) {
		vss_task_set_error(task, "buffer overflow");
		return VSS_ERROR;
	} else {
		return VSS_OK;
	}
}

static int vss_task_insert_timestamp(struct vss_task* task, uint32_t timestamp)
{
	if(vss_task_write(task, (timestamp >>  0) & 0x0000ffff)) {
		return VSS_ERROR;
	}
	if(vss_task_write(task, (timestamp >> 16) & 0x0000ffff)) {
		return VSS_ERROR;
	}

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
		if(vss_task_insert_timestamp(task, timestamp)) {
			return VSS_STOP;
		}
	}

	if(vss_task_write(task, data)) {
		return VSS_STOP;
	}

	task->write_channel += task->sweep_config->channel_step;
	if(task->write_channel >= task->sweep_config->channel_stop) {
		if(task->sweep_num > 1 || task->sweep_num < 0) {
			task->sweep_num--;
			task->write_channel = task->sweep_config->channel_start;

			return VSS_OK;
		} else {
			task->state = VSS_DEVICE_RUN_FINISHED;
			return VSS_STOP;
		}
	} else {
		return VSS_OK;
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

	int r = vss_device_run(device, task);
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
	ctx->p = 0;
	vss_buffer_read_block(&task->buffer, &ctx->data, &ctx->len);
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
	if(ctx->p >= ctx->len) {
		vss_buffer_release_block(&task->buffer);
		return VSS_STOP;
	}

	switch(task->read_state) {
		case 0:
			*timestamp = (uint16_t) ctx->data[ctx->p];
			*channel = -1;

			task->read_state = 1;
			break;
		case 1:
			*timestamp |= ctx->data[ctx->p] << 16;
			*channel = -1;

			task->read_state = 2;
			break;

		case 2:
			*power = ctx->data[ctx->p];
			*channel = task->read_channel;

			task->read_channel += task->sweep_config->channel_step;
			if(task->read_channel >= task->sweep_config->channel_stop) {
				task->read_channel = task->sweep_config->channel_start;
				task->read_state = 0;
			}
			break;
	}

	ctx->p++;
	return VSS_OK;
}

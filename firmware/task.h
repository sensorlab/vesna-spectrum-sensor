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
/** @file
 * @brief Spectrum sensing task handling.
 *
 * Typically, a user creates a task in the main thread and starts it. Task then
 * runs until it is finished (or explicitly stopped in case of tasks with
 * infinite length).
 *
 * Once the task is running, device driver uses interrupt routines to perform
 * measurements independently of the main thread and terminates a task once it
 * is finished.
 *
 * User can retrieve results asynchronously in the main thread from a circular
 * buffer while the task is running, or in case the buffer is large enough,
 * synchronously after the task has finished.
 */
#ifndef HAVE_RUN_H
#define HAVE_RUN_H

#include "vss.h"
#include "device.h"
#include "buffer.h"

/** @brief State of the task.
 *
 * During its life time, a task goes from NEW to RUNNING to FINISHED. */
enum vss_task_state {
	/** @brief A newly created task. */
	VSS_DEVICE_RUN_NEW,
	/** @brief Task currently running on a device. */
	VSS_DEVICE_RUN_RUNNING,
	VSS_DEVICE_RUN_SUSPENDED,
	/** @brief Task that is no longer running. */
	VSS_DEVICE_RUN_FINISHED
};

enum vss_task_type {
	VSS_TASK_SWEEP,
	VSS_TASK_SAMPLE
};

/** @brief Spectrum sensing task. */
struct vss_task {
	/** @brief Circular buffer for storing the measurements. */
	struct vss_buffer buffer;

	/** @brief Pointer to spectrum sweep configuration to use. */
	const struct vss_sweep_config* sweep_config;

	/** @brief Current state of the task. */
	enum vss_task_state state;

	/** @brief Remaining number of sweeps.
	 *
	 * Set to -1 for tasks that never stop. */
	volatile int sweep_num;

	/** @brief Current channel being measured. */
	unsigned int write_channel;

	/** @brief Pointer for writing to buffer. */
	power_t* write_ptr;

	/** @brief Error message for the task. */
	const char* volatile error_msg;

	enum vss_task_type type;

	unsigned int sample_num;

	unsigned int overflows;
};

/** @brief Result of a buffer read operation. */
struct vss_task_read_result {
	/** @brief Pointer for reading from buffer. */
	power_t* read_ptr;

	/** @brief Current channel being read from the buffer. */
	unsigned int read_channel;

	unsigned int read_cnt;
};

/** @brief Initialize a device task with statically allocated storage.
 *
 * Typical usage:
 *
 * @code{.c}
 * power_t data[512];
 * struct vss_task device_run;
 *
 * vss_buffer_init(&device_run, &sweep_config, sweep_num, data);
 * @endcode
 *
 * @param device_run Pointer to the task to initialize.
 * @param sweep_config Pointer to the spectrum sweep configuration to use.
 * @param sweep_num Number of spectrum sensing sweeps to perform (use -1 for infinite).
 * @param data Array to use as buffer storage.
 */
#define vss_task_init(device_run, type, sweep_config, sweep_num, data) \
	vss_task_init_size(device_run, type, sweep_config, sweep_num, data, sizeof(data))

/** @name User interface */

/** @{ */

int vss_task_init_size(struct vss_task* task, enum vss_task_type type,
		const struct vss_sweep_config* sweep_config,
		int sweep_num, power_t *data, size_t data_len);

int vss_task_start(struct vss_task* task);
int vss_task_stop(struct vss_task* task);

enum vss_task_state vss_task_get_state(struct vss_task* task);
const char* vss_task_get_error(struct vss_task* task);

void vss_task_read(struct vss_task* task, struct vss_task_read_result* ctx);
int vss_task_read_parse(struct vss_task* task, struct vss_task_read_result *ctx,
		uint32_t* timestamp, int* channel, power_t* power);

/** @} */

/** @name Device driver interface */

/** @{ */

unsigned int vss_task_get_channel(struct vss_task* task);
unsigned int vss_task_get_n_average(struct vss_task* task);
int vss_task_insert(struct vss_task* device_run, power_t data,
							uint32_t timestamp);
void vss_task_set_error(struct vss_task* task, const char* msg);

int vss_task_reserve_block(struct vss_task* task, power_t** data,
							uint32_t timestamp);
int vss_task_write_block(struct vss_task* task);

/** @} */

#endif

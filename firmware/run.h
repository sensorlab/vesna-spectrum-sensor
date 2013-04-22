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
enum vss_device_run_state {
	/** @brief A newly created task. */
	VSS_DEVICE_RUN_NEW,
	/** @brief Task currently running on a device. */
	VSS_DEVICE_RUN_RUNNING,
	/** @brief Task that is no longer running. */
	VSS_DEVICE_RUN_FINISHED
};

/** @brief Spectrum sensing task. */
struct vss_device_run {
	/** @brief Circular buffer for storing the measurements. */
	struct vss_buffer buffer;

	/** @brief Pointer to spectrum sweep configuration to use. */
	const struct vss_sweep_config* sweep_config;

	/** @brief Current state of the task. */
	enum vss_device_run_state state;

	/** @brief Remaining number of sweeps.
	 *
	 * Set to -1 for tasks that never stop. */
	volatile int sweep_num;

	/** @brief Current channel being measured. */
	unsigned int write_channel;

	/** @brief Current channel being read from the buffer. */
	unsigned int read_channel;

	/** @brief State of the buffer reader. */
	int read_state;

	/** @brief Error message for the task. */
	const char* volatile error_msg;
};

/** @brief Result of a buffer read operation. */
struct vss_device_run_read_result {
	/** @brief Pointer to the current block being read. */
	const power_t* data;

	/** @brief Length of the current block. */
	size_t len;

	/** @brief Index of the next element to parse. */
	size_t p;
};

/** @brief Initialize a device task with statically allocated storage.
 *
 * Typical usage:
 *
 * @code{.c}
 * power_t data[512];
 * struct vss_device_run device_run;
 *
 * vss_buffer_init(&device_run, &sweep_config, sweep_num, data);
 * @endcode
 *
 * @param device_run Pointer to the task to initialize.
 * @param sweep_config Pointer to the spectrum sweep configuration to use.
 * @param sweep_num Number of spectrum sensing sweeps to perform (use -1 for infinite).
 * @param data Array to use as buffer storage.
 */
#define vss_device_run_init(device_run, sweep_config, sweep_num, data) {\
	vss_buffer_init(&(device_run)->buffer, data); \
	vss_device_run_init_(device_run, sweep_config, sweep_num); \
}

/** @name User interface */

/** @{ */

void vss_device_run_init_(struct vss_device_run* device_run, const struct vss_sweep_config* sweep_config,
		int sweep_num);

int vss_device_run_start(struct vss_device_run* run);
int vss_device_run_stop(struct vss_device_run* run);

enum vss_device_run_state vss_device_run_get_state(struct vss_device_run* run);
const char* vss_device_run_get_error(struct vss_device_run* run);

void vss_device_run_read(struct vss_device_run* run, struct vss_device_run_read_result* ctx);
int vss_device_run_read_parse(struct vss_device_run* run, struct vss_device_run_read_result *ctx,
		uint32_t* timestamp, int* channel, power_t* power);

/** @} */

/** @name Device driver interface */

/** @{ */

unsigned int vss_device_run_get_channel(struct vss_device_run* device_run);
int vss_device_run_insert(struct vss_device_run* device_run, power_t data, uint32_t timestamp);
void vss_device_run_set_error(struct vss_device_run* run, const char* msg);

/** @} */

#endif

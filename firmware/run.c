#include "run.h"

/** @brief Initialize a device task.
 *
 * Note: circular buffer needs to be initialized separately. Use the
 * vss_device_run_init() macro which does that automatically.
 *
 * @param device_run Pointer to the task to initialize.
 * @param sweep_config Pointer to the spectrum sweep configuration to use.
 * @param sweep_num Number of spectrum sensing sweeps to perform (use -1 for
 * infinite).
 */
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

/** @brief Get current channel to measure.
 *
 * Called by the device driver to get the frequency channel of the next measurement.
 *
 * @param device_run Pointer to the task.
 * @return Channel number for the next measurement.
 */
unsigned int vss_device_run_get_channel(struct vss_device_run* device_run)
{
	return device_run->write_channel;
}

/** @brief Add a new measurement result for the task.
 *
 * Called by the device driver to report a new measurement.
 *
 * @param device_run Pointer to the task.
 * @param data Measurement result.
 * @param timestamp Time of the measurement.
 * @return VSS_STOP if the driver should terminate the task or VSS_OK otherwise.
 */
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

/** @brief Stop a task with an error.
 *
 * Called by the device driver in case an error was encountered and the task cannot continue.
 *
 * @param run Pointer to the task.
 * @param msg Pointer to the error message.
 */
void vss_device_run_set_error(struct vss_device_run* run, const char* msg)
{
	run->error_msg = msg;
	run->state = VSS_DEVICE_RUN_FINISHED;
}

/** @brief Retrieve an error message for a task.
 *
 * Called by the user to retrieve an error message.
 *
 * @param run Pointer to the task.
 * return Pointer to the error message or NULL if task did not encounter an error.
 */
const char* vss_device_run_get_error(struct vss_device_run* run)
{
	return run->error_msg;
}

/** @brief Start a task.
 *
 * Called by the user to start a task.
 *
 * @param run Pointer to the task that has been started.
 * @return VSS_OK on success or an error code otherwise.
 */
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

/** @brief Stop a task.
 *
 * Called by the user to force a task to stop.
 *
 * @param run Pointer to the task to stop.
 * @return VSS_OK on success or an error code otherwise.
 */
int vss_device_run_stop(struct vss_device_run* run)
{
	run->sweep_num = 0;
	return VSS_OK;
}

/** @brief Query task state.
 *
 * @param run Pointer to the task to query.
 * @return Current task state.
 */
enum vss_device_run_state vss_device_run_get_state(struct vss_device_run* run)
{
	return run->state;
}

/** @brief Read from the task's circular buffer.
 *
 * @param run Pointer to the task.
 * @param ctx Pointer to the result of the read operation.
 */
void vss_device_run_read(struct vss_device_run* run, struct vss_device_run_read_result* ctx)
{
	ctx->p = 0;
	vss_buffer_read_block(&run->buffer, &ctx->data, &ctx->len);
}

/** @brief Parse the values from the task's circular buffer.
 *
 * @param run Pointer to the task.
 * @param ctx Pointer to the result of the read operation.
 * @param timestamp Timestamp of the measurement.
 * @param channel Channel of the measurement (set to -1 if no measurement yet)
 * @param power Result of the power measurement.
 * @return VSS_STOP if there is nothing more to parse or VSS_OK otherwise.
 */
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

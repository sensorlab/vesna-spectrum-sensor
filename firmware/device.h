/** @file
 * @brief Interface definitions for a spectrum sensing device */
#ifndef HAVE_DEVICE_H
#define HAVE_DEVICE_H

#include "buffer.h"

struct vss_device_run;

/** @brief Callback for starting a spectrum sensing device.
 *
 * @param priv Pointer to the implementation specific data structure.
 * @param device_run Pointer to task to start.
 * @return VSS_OK on success or an error code otherwise. */
typedef int (*vss_device_run_t)(void* priv, struct vss_device_run* device_run);

/** @brief Callback for obtaining device status.
 *
 * Device status is returned as an implementation specific ASCII string.
 *
 * @param priv Pointer to the implementation specific data structure.
 * @param buffer Pointer to caller-allocated buffer where the result will be
 * written.
 * @param len Size of the buffer in bytes. */
typedef int (*vss_device_status_t)(void* priv, char* buffer, size_t len);

/** @brief A spectrum sensing device.
 *
 * This structure corresponds to a physical device (energy detection receiver).
 *
 * Each device can support one or more hardware configurations, described in
 * vss_device_config.
 */
struct vss_device {
	/** @brief Human-readable name of the device. */
	const char* name;

	/** @brief Callback for starting a spectrum sensing task. */
	vss_device_run_t run;

	/** @brief Callback for obtaining device status. */
	vss_device_status_t status;

	/** @brief Opaque pointer to an implementation specific data structure. */
	void* priv;
};

/** @brief Hardware configuration pre-set for a spectrum sensing device.
 *
 * @code
 * f_cmin = channel_base
 * f_cmax = channel_base + (channel_num - 1) * channel_spacing
 * @endcode
 */
struct vss_device_config {
	/** @brief Human-readable name of the pre-set. */
	const char* name;

	/** @brief Pointer to the device this pre-set belongs to. */
	const struct vss_device* device;

	/** @brief Center frequency of the first channel in Hz. */
	long long int channel_base_hz;

	/** @brief Difference between center frequencies of two adjacent
	 * channels in Hz. */
	unsigned int channel_spacing_hz;

	/** @brief Bandwidth of a channel in Hz. */
	unsigned int channel_bw_hz;

	/** @brief Total numer of channels. */
	size_t channel_num;

	/** @brief Time required for detection per channel in miliseconds
	 *
	 * Note: this is an approximate number. Accurate timestamps are
	 * returned with measurement results. */
	unsigned int channel_time_ms;

	/** @brief Opaque pointer to an implementation specific data structure. */
	void* priv;
};

/** @brief Spectrum sweep configuration.
 *
 * Describes how a spectrum sensing device sweeps the spectrum. */
struct vss_sweep_config {
	/** @brief Hardware configuration to use. */
	const struct vss_device_config *device_config;

	/** @brief Channel of the first measurement. */
	unsigned int channel_start;

	/** @brief Increment in channel number between two measurements. */
	unsigned int channel_step;

	/** @brief Channel of the one after the last measurement. */
	unsigned int channel_stop;
};

/** @brief Maximum number of device configuration that can be registered with
 * the system. */
#define VSS_MAX_DEVICE_CONFIG	10

extern int vss_device_config_list_num;
extern const struct vss_device_config* vss_device_config_list[];

int vss_device_run(const struct vss_device* device, struct vss_device_run* run);
int vss_device_status(const struct vss_device* device, char* buffer, size_t len);

int vss_device_config_add(const struct vss_device_config* device_config);
const struct vss_device_config* vss_device_config_get(int device_id, int config_id);

unsigned int vss_sweep_config_channel_num(const struct vss_sweep_config* sweep_config);

#endif

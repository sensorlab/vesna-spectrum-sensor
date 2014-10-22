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
 * @brief Interface definitions for a spectrum sensing device */
#ifndef HAVE_DEVICE_H
#define HAVE_DEVICE_H

#include "buffer.h"
#include "calibration.h"

struct vss_sweep_config;
struct vss_task;
struct vss_device_config;

/** @brief Callback for starting a spectrum sensing device.
 *
 * @param priv Pointer to the implementation specific data structure.
 * @param task Pointer to task to start.
 * @return VSS_OK on success or an error code otherwise. */
typedef int (*vss_device_run_t)(void* priv, struct vss_task* task);

/** @brief Callback for obtaining device status.
 *
 * Device status is returned as an implementation specific ASCII string.
 *
 * @param priv Pointer to the implementation specific data structure.
 * @param buffer Pointer to caller-allocated buffer where the result will be
 * written.
 * @param len Size of the buffer in bytes.
 * @return VSS_OK on success or an error code otherwise. */
typedef int (*vss_device_status_t)(void* priv, char* buffer, size_t len);

/** @brief Callback for obtaining default calibration table.
 *
 * A default calibration table for the specified device configutation is
 * returned.
 *
 * @sa calibration_set_data
 *
 * @param priv Pointer to the implementation specific data structure.
 * @param device_config Device configuration that will be used.
 * @return Pointer to calibration table on success or NULL in case of an error.
 */
typedef const struct calibration_point* (*vss_device_get_calibration_t)(
		void* priv, const struct vss_device_config* device_config);

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

	vss_device_run_t resume;

	/** @brief Callback for obtaining device status. */
	vss_device_status_t status;

	/** @brief Callback for obtaining default calibration table. */
	vss_device_get_calibration_t get_calibration;

	int supports_task_baseband;

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

	/** @brief Number of power samples to average for one data point. */
	unsigned int n_average;
};

/** @brief Maximum number of device configuration that can be registered with
 * the system. */
#define VSS_MAX_DEVICE_CONFIG	10

extern int vss_device_config_list_num;
extern const struct vss_device_config* vss_device_config_list[];

int vss_device_run_sweep(const struct vss_device* device, struct vss_task* task);
int vss_device_run_sample(const struct vss_device* device, struct vss_task* task);
int vss_device_resume(const struct vss_device* device, struct vss_task* task);
int vss_device_status(const struct vss_device* device, char* buffer, size_t len);
const struct calibration_point* vss_device_get_calibration(
		const struct vss_device* device,
		const struct vss_device_config* device_config);

int vss_device_config_add(const struct vss_device_config* device_config);
const struct vss_device_config* vss_device_config_get(int device_id, int config_id);

unsigned int vss_sweep_config_channel_num(const struct vss_sweep_config* sweep_config);

#endif

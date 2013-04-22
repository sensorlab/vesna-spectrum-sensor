#ifndef HAVE_DEVICE_H
#define HAVE_DEVICE_H

#include "buffer.h"

struct vss_device_run;

typedef int (*vss_device_run_t)(void* priv, struct vss_device_run* device_run);
typedef int (*vss_device_status_t)(void* priv, char* buffer, size_t len);

struct vss_device {
	/* Name of the device */
	const char* name;

	/* Setup a spectrum sensing scan */
	vss_device_run_t run;

	/* Start a spectrum sensing scan */
	vss_device_status_t status;

	/* Opaque pointer to a device-specific data structure */
	void* priv;
};

/* Configuration pre-set for a spectrum sensing device.
 *
 * f_cmin = channel_base
 * f_cmax = channel_base + (channel_num - 1) * channel_spacing
 *
 * resolution_bw = channel_bw
 * sweep_time = channel_time * channel_num */
struct vss_device_config {
	/* Name of the pre-set */
	const char* name;

	const struct vss_device* device;

	/* Center frequency of the first channel in Hz */
	long long int channel_base_hz;

	/* Difference between center frequencies of two adjacent 
	 * channels in Hz */
	unsigned int channel_spacing_hz;

	/* Bandwidth of a channel in Hz */
	unsigned int channel_bw_hz;

	/* Number of channels */
	size_t channel_num;

	/* Time required for detection per channel in miliseconds 
	 *
	 * Approximate number - accurate timestamps are returned with
	 * measurement results. */
	unsigned int channel_time_ms;

	/* Opaque pointer to a device-specific data structure */
	void* priv;
};

struct vss_sweep_config {
	/* Device configuration Pre-set to use */
	const struct vss_device_config *device_config;

	/* Channel of the first measurement */
	unsigned int channel_start;

	/* Increment in channel number between two measurements */
	unsigned int channel_step;

	/* Channel of the one after the last measurement */
	unsigned int channel_stop;
};

#define VSS_MAX_DEVICE_CONFIG	10

extern int vss_device_config_list_num;
extern const struct vss_device_config* vss_device_config_list[];

int vss_device_run(const struct vss_device* device, struct vss_device_run* run);
int vss_device_status(const struct vss_device* device, char* buffer, size_t len);

int vss_device_config_add(const struct vss_device_config* device_config);
const struct vss_device_config* vss_device_config_get(int device_id, int config_id);

unsigned int vss_sweep_config_channel_num(const struct vss_sweep_config* sweep_config);

#endif

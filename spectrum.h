#ifndef HAVE_SPECTRUM_H
#define HAVE_SPECTRUM_H

struct spectrum_sweep_config;

/* Return 0 to continue sweep, E_SPECTRUM_STOP_SWEEP to stop sweep and return from
 * spectrum_run or any other value on error. */
typedef int (*spectrum_cb_t)(
		/* Pointer to the sweep_config struct passed to spectrum_run */
		const struct spectrum_sweep_config* sweep_config,

		/* Timestamp of the measurement in ms since spectrum_run call */
		int timestamp,

		/* Array of measurements 
		 *
		 * data[n] = measurement for channel m, where
		 *
		 * m = channel_start + channel_step * n
		 *
		 * n = 0 .. channel_num - 1
		 */
		const int data_list[]);

struct spectrum_sweep_config {
	/* Device configuration Pre-set to use */
	const struct spectrum_dev_config *dev_config;

	/* Channel of the first measurement */
	int channel_start;

	/* Increment in channel number between two measurements */
	int channel_step;

	/* Channel of the one after the last measurement */
	int channel_stop;

	/* Constants required to calculate the measured power from the raw
	 * measurement values using the following equation:
	 *
	 * P_dbm = (X + offset) / quotient 
	 *
	 * Filled in by the device driver */
	int dbm_quotient;
	int dbm_offset;

	/* Callback function. Return -1 to stop the scan. */
	spectrum_cb_t cb;
};

/* Configuration pre-set for a spectrum sensing device.
 *
 * f_cmin = channel_base
 * f_cmax = channel_base + (channel_num - 1) * channel_spacing
 *
 * resolution_bw = channel_bw
 * sweep_time = channel_time * channel_num */
struct spectrum_dev_config {
	/* Name of the pre-set */
	const char* name;

	/* Center frequency of the first channel in Hz */
	long long int channel_base_hz;

	/* Difference between center frequencies of two adjacent 
	 * channels in Hz */
	int channel_spacing_hz;

	/* Bandwidth of a channel in Hz */
	int channel_bw_hz;

	/* Number of channels */
	int channel_num;

	/* Time required for detection per channel in miliseconds 
	 *
	 * Approximate number - accurate timestamps are returned with
	 * measurement results. */
	int channel_time_ms;

	/* Opaque pointer to a device-specific data structure */
	void* priv;
};

typedef int (*spectrum_dev_reset_t)(void* priv);
typedef int (*spectrum_dev_setup_t)(void* priv, struct spectrum_sweep_config* sweep_config);
typedef int (*spectrum_dev_run_t)(void* priv, struct spectrum_sweep_config* sweep_config);

struct spectrum_dev {
	/* Name of the device */
	const char* name;

	/* List of configuration pre-sets supported by this device */
	const struct spectrum_dev_config* const * dev_config_list;
	int dev_config_num;

	/* Reset the device */
	spectrum_dev_reset_t dev_reset;

	/* Setup a spectrum sensing scan */
	spectrum_dev_setup_t dev_setup;

	/* Start a spectrum sensing scan */
	spectrum_dev_run_t dev_run;

	/* Opaque pointer to a device-specific data structure */
	void* priv;
};

#define E_SPECTRUM_STOP_SWEEP 1
#define E_SPECTRUM_OK 0
#define E_SPECTRUM_INVALID -1
#define E_SPECTRUM_TOOMANY -2

#define SPECTRUM_MAX_DEV 10

extern int spectrum_dev_num;
extern const struct spectrum_dev* spectrum_dev_list[];

int spectrum_add_dev(const struct spectrum_dev* dev);
int spectrum_reset(void);
int spectrum_sweep_channel_num(const struct spectrum_sweep_config* sweep_config);
int spectrum_run(const struct spectrum_dev* dev, struct spectrum_sweep_config* sweep_config);
#endif

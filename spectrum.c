/* High-level interface to spectrum sensing hardware */

#include <stdlib.h>
#include "spectrum.h"

int spectrum_dev_num = 0;
const struct spectrum_dev* spectrum_dev_list[SPECTRUM_MAX_DEV];

/* Register a new spectrum sensing device to the system */
int spectrum_add_dev(const struct spectrum_dev* dev)
{
	if(spectrum_dev_num >= SPECTRUM_MAX_DEV) return E_SPECTRUM_TOOMANY;

	spectrum_dev_list[spectrum_dev_num] = dev;
	spectrum_dev_num++;

	return 0;
}

/* Reset all devices 
 *
 * Return 0 on success, or error code otherwise. */
int spectrum_reset(void)
{
	int n;
	for(n = 0; n < spectrum_dev_num; n++) {
		int r = spectrum_dev_list[n]->dev_reset(spectrum_dev_list[n]->priv);
		if(r) return r;
	}

	return 0;
}

/* Return number of channels for a sweep config. */
int spectrum_sweep_channel_num(const struct spectrum_sweep_config* sweep_config)
{
	return (sweep_config->channel_stop - sweep_config->channel_start - 1)
		/ sweep_config->channel_step + 1;
}

/* Start a spectrum sensing on a device 
 *
 * Return 0 on success, or error code otherwise. */
int spectrum_run(const struct spectrum_dev* dev, const struct spectrum_sweep_config* sweep_config)
{
	/* some sanity checks */
	if (sweep_config->channel_start >= sweep_config->channel_stop) {
		return E_SPECTRUM_INVALID;
	}

	if (sweep_config->channel_stop > sweep_config->dev_config->channel_num) {
		return E_SPECTRUM_INVALID;
	}

	if (sweep_config->cb == NULL) {
		return E_SPECTRUM_INVALID;
	}

	int r = dev->dev_setup(dev->priv, sweep_config);
	if(r) return r;

	r = dev->dev_run(dev->priv, sweep_config);
	return r;
}

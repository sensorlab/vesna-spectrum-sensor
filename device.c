#include "vss.h"
#include "device.h"

int vss_device_config_list_num;
const struct vss_device_config* vss_device_config_list[VSS_MAX_DEVICE_CONFIG];

int vss_device_config_add(const struct vss_device_config* device_config)
{
	if(vss_device_config_list_num >= VSS_MAX_DEVICE_CONFIG) return VSS_TOO_MANY;

	vss_device_config_list[vss_device_config_list_num] = device_config;
	vss_device_config_list_num++;

	return VSS_OK;
}

const struct vss_device_config* vss_device_config_get(int device_id, int config_id)
{
	int device_id_o = -1;
	int config_id_o = 0;

	const struct vss_device* device = NULL;

	int n;
	for(n = 0; n < vss_device_config_list_num; n++) {
		const struct vss_device_config* device_config = vss_device_config_list[n];

		if(device_config->device != device) {
			device = device_config->device;
			device_id_o++;
			config_id_o = 0;
		}

		if(device_id_o == device_id && config_id_o == config_id) {
			return device_config;
		}

		config_id++;
	}

	return NULL;
}

unsigned int vss_sweep_config_channel_num(const struct vss_sweep_config* sweep_config)
{
	return (sweep_config->channel_stop - sweep_config->channel_start - 1)
		/ sweep_config->channel_step + 1;
}

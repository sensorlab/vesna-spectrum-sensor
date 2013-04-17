#ifndef HAVE_AD8307_H
#define HAVE_AD8307_H

#ifdef MODEL_SNE_CREWTV
#	define AD8307_PIN_ENB	GPIO6
#	define AD8307_PIN_OUT	GPIO0
#endif

#ifdef MODEL_SNE_ISMTV_UHF
#	define AD8307_PIN_ENB	GPIO0
#	define AD8307_PIN_OUT	GPIO2
#endif

int vss_ad8307_init(void);
int vss_ad8307_power_on(void);
int vss_ad8307_power_off(void);
int vss_ad8307_get_input_power(void);

#endif

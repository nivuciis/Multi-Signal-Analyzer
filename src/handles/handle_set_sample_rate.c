#include "handles/handles_internal.h"

#include <hardware/clocks.h>
#include <hardware/vreg.h>
#include <pico/stdlib.h>

void handle_set_sample_rate(void)
{
	struct SIGROK_HANDLER *self = ana_sigrok_get_self();
	uint32_t rate = (uint32_t)strtol((char *)&self->cmd_str[1], &self->end_ptr, 10);
	if (self->end_ptr == NULL || *self->end_ptr != '\0') {
		log_warn("sigrok", "Invalid sample rate: %s", &self->cmd_str[1]);
		return;
	}
	if (rate < SIGROK_SAMPLE_RATE_MIN) {
		rate = SIGROK_SAMPLE_RATE_MIN;
	} else if (rate > SIGROK_SAMPLE_RATE_MAX) {
#if ENABLE_OVERCLOCKING
		vreg_set_voltage(VREG_VOLTAGE_1_25);
		sleep_ms(1);
		set_sys_clock_khz(250000, true);
#endif
		rate = SIGROK_SAMPLE_RATE_MAX;
	}
	self->cfg.sample_rate_hz = rate;
	log_inf("sigrok", "Sample rate: %lu Hz", (unsigned long)rate);
}

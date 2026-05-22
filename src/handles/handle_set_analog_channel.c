#include "handles/handles_internal.h"

void handle_set_analog_channel(void)
{
	struct sigrok_handler *self = ana_sigrok_get_self();

	int enable = self->cmd_str[1] - '0';
	int ch = (int)strtol((char *)&self->cmd_str[2], &self->end_ptr, 10);
	if (self->end_ptr == NULL || *self->end_ptr != '\0') {
		log_warn("sigrok", "Invalid analog channel");
		return;
	}
	if (ch >= 0 && ch <= 2) {
		if (enable) {
			self->analog_mask |= (uint8_t)(1u << ch);
		} else {
			self->analog_mask &= (uint8_t)(~(1u << ch));
		}
		log_inf("sigrok", "Analog ch %d %s", ch, enable ? "enabled" : "disabled");
	}
}

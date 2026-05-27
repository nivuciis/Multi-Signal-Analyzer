#include "handles/handles_internal.h"

void handle_set_digital_channel(void)
{
	struct sigrok_handler *self = ana_sigrok_get_self();
	int enable = self->cmd_str[1] - '0';
	int ch     = (int)strtol((char *)&self->cmd_str[2], &self->end_ptr, 10);
	if (self->end_ptr == NULL || *self->end_ptr != '\0') {
		log_warn("sigrok", "Invalid digital channel");
		return;
	}
	if (ch >= 0 && ch < MAX_NUM_CHANNELS) {
		if (enable) {
			self->digital_mask |= (uint16_t)(1u << ch);
		} else {
			self->digital_mask &= (uint16_t)(~(1u << ch));
		}
		log_inf("sigrok", "Digital ch %d %s", ch, enable ? "enabled" : "disabled");
	}
}

#include "handles/handles_internal.h"

void handle_set_pretrigger(void)
{
	struct sigrok_handler *self = ana_sigrok_get_self();
	uint32_t depth = (uint32_t)strtol((char *)&self->cmd_str[1], &self->end_ptr, 10);

	if (self->end_ptr == (char *)&self->cmd_str[1] || self->end_ptr == NULL ||
	    *self->end_ptr != '\0') {
		log_warn("sigrok", "Invalid pretrigger depth: %s", &self->cmd_str[1]);
		self->response[0] = '\0';
		return;
	}

	self->pretrigger_samples = depth;
	log_inf("sigrok", "Pretrigger samples: %lu", (unsigned long)depth);
}

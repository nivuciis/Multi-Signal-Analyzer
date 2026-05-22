#include "handles/handles_internal.h"

void handle_set_sample_limit(void)
{
	struct sigrok_handler *self = ana_sigrok_get_self();
	uint32_t limit = (uint32_t)strtol((char *)&self->cmd_str[1], &self->end_ptr, 10);

	if (self->end_ptr == NULL || *self->end_ptr != '\0') {
		log_warn("sigrok", "Invalid sample limit: %s", &self->cmd_str[1]);
		return;
	}

	if (limit > SIGROK_SAMPLE_LIMIT_MAX) {
		limit = SIGROK_SAMPLE_LIMIT_MAX;
	}

	if (limit == 0) {
		limit = 1;
	}

	self->cfg.samples = limit;
	self->num_samples = limit;

	log_inf("sigrok", "Sample limit: %lu", (unsigned long)limit);
}

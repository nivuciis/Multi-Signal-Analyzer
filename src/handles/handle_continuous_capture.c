#include "handles/handles_internal.h"

/* TODO: true double-buffer streaming @JoaoMatheusND */
void handle_continuous_capture(void)
{
	struct sigrok_handler *self = ana_sigrok_get_self();

	run_capture(true);
	self->response[0] = '\0';
}

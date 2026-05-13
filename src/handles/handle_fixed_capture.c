#include "handles/handles_internal.h"

void handle_fixed_capture(void)
{
	struct sigrok_handler *self = ana_sigrok_get_self();

	run_capture(false);
	/* The done marker $<n>+\0 is the complete response for capture commands.
	 * Suppress the generic '*' ACK so the driver does not receive an extra
	 * byte after the protocol stop marker. */
	self->response[0] = '\0';
}

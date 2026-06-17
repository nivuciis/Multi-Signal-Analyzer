#include "handles/handles_internal.h"

void handle_get_analog_scale(void)
{
	struct sigrok_handler *self = ana_sigrok_get_self();
	int ch = (int)strtol((char *)&self->cmd_str[1], &self->end_ptr, 10);
	if (self->end_ptr == (char *)&self->cmd_str[1] || self->end_ptr == NULL ||
	    *self->end_ptr != '\0') {
		ana_send_response("ERR");
		self->response[0] = '\0'; /* no success ACK after an error reply */
		return;
	}
	if (ch >= 0 && ch <= 2) {
		ana_send_response(SIGROK_ANALOG_SCALE);
	} else {
		ana_send_response("ERR");
		self->response[0] = '\0';
	}
}

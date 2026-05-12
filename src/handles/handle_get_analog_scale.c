#include "handles/handles_internal.h"

void handle_get_analog_scale(void)
{
	int ch = (int)strtol((char *)&self.cmd_str[1], &self.end_ptr, 10);
	if (self.end_ptr == NULL || *self.end_ptr != '\0') {
		ana_send_response("ERR");
		return;
	}
	ana_send_response((ch >= 0 && ch <= 2) ? SIGROK_ANALOG_SCALE : "ERR");
}

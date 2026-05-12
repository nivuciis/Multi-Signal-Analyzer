#include "handles/handles_internal.h"

/* TODO: true double-buffer streaming @JoaoMatheusND */
void handle_continuous_capture(void)
{
	run_capture(true);
	self.response[0] = '\0';
}

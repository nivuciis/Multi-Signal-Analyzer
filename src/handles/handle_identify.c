#include "handles/handles_internal.h"
#include "led.h"

#define SIGROK_IDENT_STRING "SRPICO,A031D12,02"

void handle_identify(void)
{
	ana_send_response(SIGROK_IDENT_STRING);
	ana_led_set_status(LED_STATUS_CONNECTED);
}

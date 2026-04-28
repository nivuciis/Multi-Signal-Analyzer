/*******************************************************************
 * @file main.c
 *
 * @brief Main file for the workstation manager application.
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @author Vinicius Rafael Marques de Carvalho (vinicius.carvalho@edge.ufal.br)
 * @version 0.1
 * @date 15/01/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#include "capture_data.h"
#include "channels.h"
#include "led.h"
#include "sigrok_handler.h"
#include "adc.h"

#include <hardware/timer.h>
#include <pico/stdlib.h>
#include <pico/time.h>
#include <tusb.h>

static char buf[64];
/**
 * @brief Callback to synchronize the LED status with the USB connection state
 *
 */
static bool ana_sync_led_with_usb_connnection(struct repeating_timer *rt)
{
	bool is_usb_connected = tud_cdc_connected();
	ana_led_set_status((is_usb_connected) ? LED_STATUS_CONNECTED : LED_STATUS_OFF);
	return true;
}

int main()
{
	ana_led_init();
	tusb_init();
	ana_sigrok_handle_init();
	ana_channels_init(pio0);
	ana_adc_init();

	struct repeating_timer usb_conection_timer;

	if (ana_capture_init(ana_channels_get_module()) != PICO_OK) {
		ana_led_set_status(LED_STATUS_ERROR);
		return PICO_ERROR_IO;
	}
	
	add_repeating_timer_ms(100, ana_sync_led_with_usb_connnection, NULL, &usb_conection_timer);

	while (1) {
		tud_task();

		if (tud_cdc_available()) {
			uint32_t count = tud_cdc_read(buf, sizeof(buf));
			for (uint32_t i = 0; i < count; i++) {
				ana_sigrok_handle_process_byte(buf[i]);
			}
		}
		__wfi();
	}

	return 0;
}

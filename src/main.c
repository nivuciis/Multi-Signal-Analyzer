/*******************************************************************
 * @file main.c
 *
 * @brief Main file for the workstation manager application.
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @author Vinicius Rafael Marques de Carvalho (vinicius.carvalho@edge.ufal.br)
 * @version 0.1
 * @date 07/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/

#include "led.h"

#include <pico/stdlib.h>
#include <pico/time.h>
#include <tusb.h>

static void sync_led_with_usb_connection()
{
	bool is_usb_connected = tud_cdc_connected();
	if (is_usb_connected) {
		ana_led_set_status(LED_STATUS_CONNECTED);
	} else {
		ana_led_set_status(LED_STATUS_OFF);
	}
}

int main()
{

	ana_led_init();
	tusb_init();
	uint32_t led_timer = 0;

	while (1) {
		tud_task();

		if (to_ms_since_boot(get_absolute_time()) - led_timer > 30) {
			led_timer = to_ms_since_boot(get_absolute_time());
			sync_led_with_usb_connection();
		}
	}

	return 0;
}

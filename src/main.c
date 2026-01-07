/*******************************************************************
 * @file main.c
 *
 * @brief Main file for the workstation manager application.
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 07/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/

#include "led.h"

#include <pico/stdio_usb.h>
#include <pico/time.h>

static bool is_usb_connected = false;

static void sync_led_with_usb_connection()
{
    is_usb_connected = stdio_usb_connected();
    ana_led_set_status((is_usb_connected) ? LED_STATUS_CONNECTED : LED_STATUS_OFF);
}

int main()
{
	stdio_init_all();
	ana_led_init();

	while (1) {
		sync_led_with_usb_connection();
        sleep_ms(30);
	}

	return 0;
}

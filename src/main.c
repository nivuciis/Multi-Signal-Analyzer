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

/*
 * Multicore architecture
 * ----------------------
 * Core 0 — USB focused:
 *   tud_task() loop + tud_cdc_read() → usb_util rx_ring
 *                                    + usb_util tx_ring → tud_cdc_write()
 *
 * Core 1 — Sigrok processing:
 *   usb_util rx_ring → ana_sigrok_handle_process_byte() → usb_util tx_ring
 *                                                          (via ana_usb_write)
 *
 */

#include "adc.h"
#include "capture_data.h"
#include "channels.h"
#include "device/usbd.h"
#include "led.h"
#include "rs485.h"
#include "handles/sigrok_handler.h"
#include "usb_util.h"
#include <stdint.h>

#include <hardware/timer.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <pico/time.h>
#include <pico/types.h>
#include <tusb.h>

static bool ana_sync_led_with_usb_connection(struct repeating_timer *rt)
{
	bool connected = tud_cdc_connected();
	ana_usb_set_connected(connected);
	ana_led_set_status(connected ? LED_STATUS_CONNECTED : LED_STATUS_OFF);
	return true;
}

void ana_core1_entry(void)
{
	uint8_t byte;

	while (1) {
		if (ana_usb_rx_read(&byte)) {
			ana_sigrok_handle_process_byte(byte);
		} else {
			tight_loop_contents();
		}
	}
}

int main(void)
{
	multicore_reset_core1();

	ana_led_init();
	tusb_init();
	ana_sigrok_handle_init();
	ana_channels_init(pio0);
	ana_rs485_init(pio1);
	ana_adc_init();

	multicore_launch_core1(ana_core1_entry);

	struct repeating_timer usb_connection_timer;

	if (ana_capture_init(ana_channels_get_module()) != PICO_OK) {
		ana_led_set_status(LED_STATUS_ERROR);
		return PICO_ERROR_IO;
	}

	if (ana_capture_init(ana_rs485_get_module()) != PICO_OK) {
		ana_led_set_status(LED_STATUS_ERROR);
		return PICO_ERROR_IO;
	}

	add_repeating_timer_ms(100, ana_sync_led_with_usb_connection, NULL,
			       &usb_connection_timer);

	uint8_t tmp[64];

	while (1) {
		tud_task();

		/* CDC → RX ring (Core 1 will consume) */
		if (tud_cdc_available()) {
			uint32_t count = tud_cdc_read(tmp, sizeof(tmp));
			ana_usb_rx_write(tmp, count);
		}

		/* TX ring → CDC (Core 1 produced, we send) */
		ana_usb_tx_drain();
	}

	return 0;
}

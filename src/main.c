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

#include "led.h"
#include "capture_data.h"
#include "sigrok_handler.h"

#include <pico/stdlib.h>
#include <pico/time.h>
#include <tusb.h>

static bool is_usb_connected = false;

/**
 * @brief Synchronize LED status with USB connection state
 * 
 */
static void ana_sync_led_with_usb()
{
	bool is_usb_connected = tud_cdc_connected();
	ana_led_set_status((is_usb_connected) ? LED_STATUS_CONNECTED : LED_STATUS_OFF);
}

/**
 * @brief Callback function for handling received data over USB CDC
 * 
 * @param msg The received message 
 */
void tud_cdc_rx_cb(uint8_t msg)
{
    (void)msg;

    if (!tud_cdc_available()) return;

    char buf[64];
    uint32_t count = tud_cdc_read(buf, sizeof(buf));

    for (uint32_t i = 0; i < count; i++) {
        sigrok_process_byte(buf[i]);
    }
}

int main()
{

	ana_led_init();
	tusb_init();
	sigrok_init();

    if (ana_capture_init() != PICO_OK) {
        ana_led_set_status(LED_STATUS_ERROR); 
    }
	uint32_t led_timer = 0;

	while (1) {
		tud_task();
		
		if (to_ms_since_boot(get_absolute_time()) - led_timer > 100) {
            led_timer = to_ms_since_boot(get_absolute_time());
            if (ana_led_get_status() != LED_STATUS_CAPTURING) {
                ana_sync_led_with_usb();
            }
        }
	}

	return 0;
}

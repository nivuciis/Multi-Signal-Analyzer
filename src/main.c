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
 *   tud_task() loop + tud_cdc_read() → rx_ring + tx_ring → tud_cdc_write()
 *
 * Core 1 — Sigrok processing:
 *   rx_ring → ana_sigrok_handle_process_byte() → tx_ring (via ana_usb_write)
 *
 * Two lock-free SPSC (single-producer / single-consumer) ring buffers
 * connect the two cores without needing a mutex:
 *
 *   rx_ring  — Core 0 produces (from USB CDC), Core 1 consumes.
 *   tx_ring  — Core 1 produces (sigrok responses), Core 0 consumes.
 *
 * Both ring sizes must be a power of 2.
 */

#include "adc.h"
#include "capture_data.h"
#include "channels.h"
#include "led.h"
#include "sigrok_handler.h"
#include "usb_comm.h"
#include <stdint.h>
#include <string.h>

#include <hardware/timer.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <pico/time.h>
#include <pico/types.h>
#include <tusb.h>


#define RX_RING_SIZE 512U    /**< Bytes: Core 0 → Core 1 (received commands) */
#define TX_RING_SIZE 32768U  /**< Bytes: Core 1 → Core 0 (responses / sample data) */


static uint8_t rx_ring[RX_RING_SIZE];
static volatile uint32_t rx_head = 0; /**< Written by Core 0 */
static volatile uint32_t rx_tail = 0; /**< Read    by Core 1 */

static uint8_t tx_ring[TX_RING_SIZE];
static volatile uint32_t tx_head = 0; /**< Written by Core 1 */
static volatile uint32_t tx_tail = 0; /**< Read    by Core 0 */

static volatile bool usb_connected = false;


static void rx_ring_write(const uint8_t *data, uint32_t len)
{
	uint32_t head = rx_head;

	for (uint32_t i = 0; i < len; i++) {
		uint32_t next = (head + 1u) & (RX_RING_SIZE - 1u);
		if (next == rx_tail) {
			break; /* ring full — drop remainder */
		}
		rx_ring[head] = data[i];
		head = next;
	}

	rx_head = head;
}


static inline bool rx_ring_read(uint8_t *byte)
{
	uint32_t tail = rx_tail;

	if (tail == rx_head) {
		return false;
	}

	*byte = rx_ring[tail];

	rx_tail = (tail + 1u) & (RX_RING_SIZE - 1u);
	return true;
}

bool ana_usb_write(const uint8_t *buf, uint32_t len)
{
	uint32_t head = tx_head;

	for (uint32_t i = 0; i < len; i++) {
		uint32_t next = (head + 1u) & (TX_RING_SIZE - 1u);

		/* Spin until Core 0 drains enough space; abort if disconnected */
		while (next == tx_tail) {
			if (!usb_connected) {
				return false;
			}
		}

		tx_ring[head] = buf[i];
		head = next;
	}

	tx_head = head;
	return usb_connected;
}

bool ana_usb_is_connected(void)
{
	return usb_connected;
}

static void tx_ring_drain(void)
{
	uint32_t head = tx_head;
	uint32_t tail = tx_tail;
	bool flushed  = false;

	while (tail != head) {
		uint32_t avail = tud_cdc_write_available();
		if (avail == 0) {
			break;
		}

		uint32_t pending = (head - tail) & (TX_RING_SIZE - 1u);
		uint32_t linear  = TX_RING_SIZE - tail;
		uint32_t chunk   = pending < linear ? pending : linear;
		chunk = chunk < avail ? chunk : avail;

		tud_cdc_write(&tx_ring[tail], chunk);
		tail    = (tail + chunk) & (TX_RING_SIZE - 1u);
		flushed = true;
	}

	if (flushed) {
		tud_cdc_write_flush();
		tx_tail = tail;
	}
}

static bool ana_sync_led_with_usb_connection(struct repeating_timer *rt)
{
	usb_connected = tud_cdc_connected();
	ana_led_set_status(usb_connected ? LED_STATUS_CONNECTED : LED_STATUS_OFF);
	return true;
}

void ana_core1_entry(void)
{
	uint8_t byte;

	while (1) {
		if (rx_ring_read(&byte)) {
			ana_sigrok_handle_process_byte(byte);
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
	ana_adc_init();

	multicore_launch_core1(ana_core1_entry);

	struct repeating_timer usb_connection_timer;

	if (ana_capture_init(ana_channels_get_module()) != PICO_OK) {
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
			rx_ring_write(tmp, count);
		}

		/* TX ring → CDC (Core 1 produced, we send) */
		tx_ring_drain();
	}

	return 0;
}

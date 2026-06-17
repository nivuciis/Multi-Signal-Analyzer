/*******************************************************************
 * @file usb_util.c
 *
 * @brief USB ring-buffer utilities for inter-core communication.
 *
 * Owns both the RX ring (Core 0 → Core 1) and the TX ring
 * (Core 1 → Core 0).  Core 0 calls ana_usb_rx_write() and
 * ana_usb_tx_drain(); Core 1 calls ana_usb_rx_read() and
 * ana_usb_write().
 *
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @author Vinicius Rafael Marques de Carvalho (vinicius.carvalho@edge.ufal.br)
 * @version 0.5
 * @date 07/05/2026
 *
 * @note Ring buffer is used to decouple the USB CDC processing (Core 0) from the Sigrok protocol handling (Core 1).  
 * 		This allows Core 1 to process data at its own pace without blocking Core 0's USB tasks, and vice versa.
 *
 * @note the __dmb() calls ensure memory ordering between the cores, preventing race conditions when checking connection status and updating ring buffer indices.
 *		Ensuring that all memory accesses prior to the __dmb() call occur before subsequent instructions. Blocks CPU optimizations that could reorder memory accesses 
 *		across the barrier, which is crucial for correct synchronization between the two cores when checking connection status and updating ring buffer indices.
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#include "usb_util.h"

#include <hardware/sync.h>
#include <pico/platform/common.h>
#include <stdint.h>
#include <tusb.h>

#define RX_RING_SIZE 512U
#define TX_RING_SIZE 32768U

static uint8_t rx_ring[RX_RING_SIZE];
static volatile uint32_t rx_head = 0; /**< Written by Core 0 */
static volatile uint32_t rx_tail = 0; /**< Read    by Core 1 */

static uint8_t tx_ring[TX_RING_SIZE];
static volatile uint32_t tx_head = 0; /**< Written by Core 1 */
static volatile uint32_t tx_tail = 0; /**< Read    by Core 0 */

static volatile bool usb_connected = false;
static volatile bool abort_request = false;

bool ana_usb_is_connected(void)
{
	__dmb();
	return usb_connected;
}

void ana_usb_request_abort(void)
{
	__dmb();
	abort_request = true;
	__dmb();
}

bool ana_usb_abort_requested(void)
{
	__dmb();
	return abort_request;
}

void ana_usb_clear_abort(void)
{
	__dmb();
	abort_request = false;
	__dmb();
}

void ana_usb_set_connected(bool connected)
{
	__dmb();
	usb_connected = connected;
	__dmb();
}

void ana_usb_rx_write(const uint8_t *data, uint32_t len)
{
	uint32_t head = rx_head;

	for (uint32_t i = 0; i < len; i++) {
		uint32_t next = (head + 1u) & (RX_RING_SIZE - 1u);

		while (next == rx_tail) {
			__dmb();
			if (!ana_usb_is_connected()) {
				__dmb();
				rx_head = head;
				return;
			}
			tud_task();
			/* Core 1 may be blocked in ana_usb_write() waiting for TX
			 * space; keep draining here or both cores deadlock when the
			 * host floods RX while a capture is streaming. */
			ana_usb_tx_drain();
			tight_loop_contents();
		}

		rx_ring[head] = data[i];
		head = next;
	}

	__dmb();
	rx_head = head;
}

bool ana_usb_rx_read(uint8_t *byte)
{
	uint32_t tail = rx_tail;
	uint32_t head = rx_head;
	__dmb();

	if (tail == head) {
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
			__dmb();
			if (!ana_usb_is_connected()) {
				return false;
			}
			tight_loop_contents();
		}

		tx_ring[head] = buf[i];
		head = next;
	}

	__dmb();
	tx_head = head;
	return ana_usb_is_connected();
}

void ana_usb_tx_drain(void)
{
	__dmb();
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

		uint32_t written = tud_cdc_write(&tx_ring[tail], chunk);
		if (written == 0) {
			break;
		}
		tail    = (tail + written) & (TX_RING_SIZE - 1u);
		flushed = true;
	}

	if (flushed) {
		tud_cdc_write_flush();

		__dmb();
		tx_tail = tail;
	}
}

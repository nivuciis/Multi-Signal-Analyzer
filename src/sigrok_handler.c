/*******************************************************************
 * @file sigrok_handler.c
 *
 * @brief Handles the Sigrok protocol communication over USB CDC.
 *
 * Protocol reference: https://github.com/pico-coder/sigrok-pico
 *
 * @author Vinicius Rafael Marques de Carvalho (vinicius.carvalho@edge.ufal.br)
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.5
 * @date 23/03/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#include "adc.h"
#include "capture_data.h"
#include "channels.h"
#include "handles/handles.h"
#include "handles/handles_internal.h"
#include "led.h"
#include "log.h"
#include "usb_util.h"

#include <stdio.h>

#define CAPTURE_CHUNK_SIZE 1024U

static struct sigrok_handler self = {
	.sample_rate = 5000,
	.num_samples = 1024,
	.digital_mask = DIGITAL_MASK_DEFAULT,
	.analog_mask = ANALOG_MASK_DEFAULT,
	.analog_channel = 0,
	.digital_channel = 0,
	.digital_bits_per_transfer = 2,
	.cmd_str_index = 0,
	.cmd_str = {0},
	.response = {0},
	.last_was_cr = false,
	.tx = {0},
	.cfg =
		{
			.sample_rate_hz = 5000,
			.samples = 1000,
		},
	.trigger_config =
		{
			.trigger_mask = 0x0000,
			.trigger_type = {-1},
		},
};

typedef struct {
	uint8_t command;
	void (*handler)(void);
} sigrok_command_t;

struct tx_stream {
	uint32_t idx;
	uint32_t bytes_sent;
};

static uint8_t active_analog_channels[ADC_NUM_CHANNELS];
static uint8_t active_analog_count;

void ana_send_response(const char *str)
{
	if (!ana_usb_is_connected()) {
		log_debug("sigrok", "Cannot send (disconnected): %s", str);
		return;
	}
	if (!ana_usb_write((const uint8_t *)str, strlen(str))) {
		self.response[0] = '\0';
		self.last_was_cr = false;
		self.cmd_str_index = 0;
	}
}

static bool ana_send_bytes(const uint8_t *buf, uint32_t len)
{
	if (!ana_usb_is_connected()) {
		return false;
	}
	return ana_usb_write(buf, len);
}

static void tx_init(void)
{
	int highest = -1;
	for (int i = 15; i >= 0; i--) {
		if (self.digital_mask & (1u << i)) {
			highest = i;
			break;
		}
	}

	self.tx.bytes_per_dig_sample = (highest < 0) ? 0 : (highest < 7) ? 1 : 2;
	self.digital_bits_per_transfer = self.tx.bytes_per_dig_sample;
	self.tx.active_analog_ch = ana_capture_data_get_analog_channels_count(self.analog_mask);
	self.tx.bytes_per_sample = self.tx.bytes_per_dig_sample + self.tx.active_analog_ch;
}

static inline void tx_build_active_analog_list(void)
{
	active_analog_count = 0;

	for (uint8_t i = 0; i < ADC_NUM_CHANNELS; i++) {
		if (self.analog_mask & (1u << i)) {
			active_analog_channels[active_analog_count] = i;
			active_analog_count++;
		}
	}
}

static inline bool tx_flush(struct tx_stream *tx)
{
	if (tx->idx == 0) {
		return true;
	}

	if (!ana_send_bytes(self.tx.buf, tx->idx)) {
		return false;
	}

	tx->bytes_sent += tx->idx;
	tx->idx = 0;

	return true;
}

static inline bool tx_reserve(struct tx_stream *tx, uint32_t needed)
{
	if ((tx->idx + needed) >= TX_BUF_SIZE) {
		return tx_flush(tx);
	}

	return true;
}

static bool ana_send_packet_channels(const uint16_t *dig_buf, uint32_t chunk_samples,
				     uint32_t *bytes_out)
{
	struct ana_adc_module *adc = ana_adc_get_module();

	const uint16_t *dig = dig_buf;

	struct tx_stream tx = {
		.idx = 0,
		.bytes_sent = 0,
	};

	tx_build_active_analog_list();

	if (chunk_samples == 0) {
		*bytes_out = 0;
		return true;
	}

	if (active_analog_count > 0) {

		for (uint32_t i = 0; i < chunk_samples; i++) {

			uint16_t sample = (uint16_t)((dig[i] >> 4) & self.digital_mask);

			uint32_t needed = self.tx.bytes_per_dig_sample + active_analog_count;

			if (!tx_reserve(&tx, needed)) {
				return false;
			}

			/*
			 * Digital sample bytes
			 */
			if (self.tx.bytes_per_dig_sample >= 1) {
				self.tx.buf[tx.idx++] = (uint8_t)(0x80u | (sample & 0x7Fu));
			}

			if (self.tx.bytes_per_dig_sample >= 2) {
				self.tx.buf[tx.idx++] = (uint8_t)(0x80u | ((sample >> 7) & 0x7Fu));
			}

			/*
			 * Analog bytes
			 */
			for (uint8_t ch = 0; ch < active_analog_count; ch++) {

				self.tx.buf[tx.idx++] =
					ana_adc_sigrok_byte(active_analog_channels[ch], i, adc);
			}
		}

		if (!tx_flush(&tx)) {
			return false;
		}

		*bytes_out = tx.bytes_sent;

		return true;
	}

	for (uint32_t i = 0; i < chunk_samples; i++) {

		uint16_t cur = (uint16_t)((dig[i] >> 4) & self.digital_mask);

		if (!tx_reserve(&tx, self.tx.bytes_per_dig_sample)) {
			return false;
		}

		if (self.tx.bytes_per_dig_sample >= 1) {
			self.tx.buf[tx.idx++] = (uint8_t)(0x80u | (cur & 0x7Fu));
		}

		if (self.tx.bytes_per_dig_sample >= 2) {
			self.tx.buf[tx.idx++] = (uint8_t)(0x80u | ((cur >> 7) & 0x7Fu));
		}
	}

	if (!tx_flush(&tx)) {
		return false;
	}

	*bytes_out = tx.bytes_sent;

	return true;
}

static bool capture_chunks(struct ana_module_system *config, uint32_t n_samples,
			   uint32_t *total_sent)
{
	uint32_t remaining = n_samples;

	/*
	 * Ping-pong DMA buffers: while DMA fills cap_buf, the CPU sends the
	 * previously captured tx_buf over USB.  The TX ring is drained by Core 0
	 * asynchronously, so ana_send_packet_channels() returns as soon as data
	 * is queued — the actual USB transfer overlaps the next DMA capture.
	 */
	uint16_t *buf[2] = {config->dma.dma_buffer, ana_channels_get_alt_buffer()};
	int cap_idx = 0;
	uint16_t *tx_buf = NULL;
	uint32_t tx_samples = 0;

	while (remaining > 0 && ana_usb_is_connected()) {
		uint32_t chunk = (remaining < CAPTURE_CHUNK_SIZE) ? remaining : CAPTURE_CHUNK_SIZE;

		self.cfg.samples = chunk;
		ana_module_set_sample_rate(config);

		/* Arm DMA into the capture buffer */
		config->dma.dma_buffer = buf[cap_idx];
		memset(buf[cap_idx], 0, chunk * sizeof(uint16_t));
		ana_capture_data_start(config);

		/* Send previous chunk while DMA runs */
		if (tx_buf != NULL) {
			uint32_t chunk_bytes = 0;
			if (!ana_send_packet_channels(tx_buf, tx_samples, &chunk_bytes)) {
				ana_module_pio_dma_abort(config);
				config->dma.dma_buffer = buf[0];
				return false;
			}
			*total_sent += chunk_bytes;
		}

		/* Wait for DMA, aborting on USB disconnect */
		if (!ana_capture_data_wait(config)) {
			config->dma.dma_buffer = buf[0];
			return false;
		}

		/* ADC capture happens after digital DMA (sequential per chunk) */
		if (self.tx.active_analog_ch > 0) {
			ana_adc_capture_dma(chunk, self.analog_mask);
		}

		tx_buf = buf[cap_idx];
		tx_samples = chunk;
		cap_idx ^= 1;
		remaining -= chunk;
	}

	/* Send the final captured chunk */
	if (tx_buf != NULL && ana_usb_is_connected()) {
		uint32_t chunk_bytes = 0;
		if (!ana_send_packet_channels(tx_buf, tx_samples, &chunk_bytes)) {
			config->dma.dma_buffer = buf[0];
			return false;
		}
		*total_sent += chunk_bytes;
	}

	config->dma.dma_buffer = buf[0];
	return true;
}

void run_capture(bool continuous)
{
	struct ana_module_system *config = ana_channels_get_module();

	tx_init();
	ana_led_set_status(LED_STATUS_CAPTURING);

	do {
		uint32_t total_sent = 0;
		bool ok = true;

		uint32_t pretrigger = (self.pretrigger_samples < self.num_samples &&
				       self.trigger_config.trigger_mask != 0)
					      ? self.pretrigger_samples
					      : 0;
		uint32_t post_samples = self.num_samples - pretrigger;

		/* Phase 1: pretrigger — simple capture with no trigger wait */
		if (pretrigger > 0) {
			uint16_t saved_mask = self.trigger_config.trigger_mask;
			self.trigger_config.trigger_mask = 0;
			ana_channels_apply_trigger();
			self.trigger_config.trigger_mask = saved_mask;
			ana_module_set_sample_rate(config);

			ok = capture_chunks(config, pretrigger, &total_sent);
		}

		/* Phase 2: trigger + post-trigger capture */
		if (ok) {
			ana_channels_apply_trigger();
			ana_module_set_sample_rate(config);

			if (self.trigger_config.trigger_mask != 0 &&
			    post_samples > CAPTURE_CHUNK_SIZE) {
				/*
				 * First chunk: PIO runs trigger-detection program and
				 * waits for the trigger event before sampling.  DMA
				 * stalls on DREQ during the wait, so all CAPTURE_CHUNK_SIZE
				 * transfers are genuine post-trigger samples.
				 */
				ok = capture_chunks(config, CAPTURE_CHUNK_SIZE, &total_sent);

				/*
				 * Remaining chunks: switch to the simple capture program so
				 * subsequent iterations do not re-arm the trigger and stall
				 * waiting for another edge/level instead of streaming data.
				 */
				if (ok) {
					ana_channels_load_simple();
					ok = capture_chunks(config,
							    post_samples - CAPTURE_CHUNK_SIZE,
							    &total_sent);
				}
			} else {
				ok = capture_chunks(config, post_samples, &total_sent);
			}
		}

		self.cfg.samples = self.num_samples;

		if (!ok) {
			ana_usb_write((const uint8_t *)"!!!$0+", 6U);
			log_warn("sigrok", "Capture aborted: host disconnected");
			break;
		}

		char done_marker[32];

		snprintf(done_marker, sizeof(done_marker), "$%lu+", (unsigned long)total_sent);
		ana_usb_write((const uint8_t *)done_marker, strlen(done_marker));

	} while (continuous && ana_usb_is_connected());

	ana_led_set_status(LED_STATUS_CONNECTED);
}

struct sigrok_trigger *ana_sigrok_get_trigger(void)
{
	return &self.trigger_config;
}

static const sigrok_command_t sigrok_commands[] = {
	{SIGROK_CMD_IDENTIFY, handle_identify},
	{SIGROK_CMD_SET_SAMPLE_RATE, handle_set_sample_rate},
	{SIGROK_CMD_SET_SAMPLE_LIMIT, handle_set_sample_limit},
	{SIGROK_CMD_GET_ANALOG_SCALE, handle_get_analog_scale},
	{SIGROK_CMD_SET_ANALOG_CHANNEL, handle_set_analog_channel},
	{SIGROK_CMD_SET_DIGITAL_CHANNEL, handle_set_digital_channel},
	{SIGROK_CMD_FIXED_CAPTURE, handle_fixed_capture},
	{SIGROK_CMD_CONTINUOUS_CAPTURE, handle_continuous_capture},
	{SIGROK_CMD_SET_PRETRIGGER, handle_set_pretrigger},
	{SIGROK_CMD_SET_TRIGGER, handle_set_trigger}};

#define SIGROK_COMMAND_COUNT (sizeof(sigrok_commands) / sizeof(sigrok_commands[0]))

void ana_sigrok_handle_init(void)
{
	self.cmd_str_index = 0;
	memset(self.cmd_str, 0, sizeof(self.cmd_str));
	self.trigger_config.trigger_mask = 0;
	self.analog_mask = ANALOG_MASK_DEFAULT;
	self.digital_mask = DIGITAL_MASK_DEFAULT;
	self.pretrigger_samples = 0;
	self.last_was_cr = false;
}

void ana_sigrok_handle_process_byte(uint8_t received_command)
{
	if (received_command == '\n' && self.last_was_cr) {
		self.last_was_cr = false;
		return;
	}
	self.last_was_cr = (received_command == '\r');

	memset(self.response, 0, sizeof(self.response));
	self.response[0] = '\0';

	if (received_command == '*') {
		ana_sigrok_handle_init();
		ana_led_set_status(LED_STATUS_OFF);
		return;
	}

	if (received_command == '\r' || received_command == '\n') {
		self.cmd_str[self.cmd_str_index] = '\0';
		strcpy((char *)self.response, "*");

		for (size_t i = 0; i < SIGROK_COMMAND_COUNT; i++) {
			if (self.cmd_str[0] == sigrok_commands[i].command) {
				sigrok_commands[i].handler();
				break;
			}
		}

		if (self.response[0] != '\0') {
			ana_send_response((char *)self.response);
		}

		self.cmd_str_index = 0;

	} else if (self.cmd_str_index < (int8_t)(sizeof(self.cmd_str) - 1)) {
		self.cmd_str[self.cmd_str_index] = (char)received_command;
		self.cmd_str_index++;
	} else {
		log_warn("sigrok", "Command buffer overflow, resetting");
		self.cmd_str_index = 0;
	}
}

struct pulseview_sample_config *ana_sigrok_get_sample_config(void)
{
	return &self.cfg;
}

struct sigrok_handler *ana_sigrok_get_self(void)
{
	return &self;
}

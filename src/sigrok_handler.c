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

#include "handles/handles_internal.h"
#include "handles/handles.h"

#include <stdio.h>

#define CAPTURE_CHUNK_SIZE 1024U
#define FLUSH_THRESHOLD    256U
#define RLE_BASE           0x30U
#define RLE_MAX_RUN        32U

struct SIGROK_HANDLER self = {
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

void ana_send_response(const char *str)
{
	if (!ana_usb_is_connected()) {
		log_debug("sigrok", "Cannot send (disconnected): %s", str);
		return;
	}
	ana_usb_write((const uint8_t *)str, strlen(str));
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

	self.tx.bytes_per_dig_sample = (highest < 0) ? 0 : (highest < 8) ? 1 : 2;
	self.digital_bits_per_transfer = self.tx.bytes_per_dig_sample;
	self.tx.active_analog_ch = ana_capture_data_get_analog_channels_count(self.analog_mask);
	self.tx.bytes_per_sample = self.tx.bytes_per_dig_sample + self.tx.active_analog_ch;
}

typedef struct {
	uint32_t idx;
	uint32_t bytes_sent;
} tx_stream_t;

static uint8_t active_analog_channels[ADC_NUM_CHANNELS];
static uint8_t active_analog_count;

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

static inline bool tx_flush(tx_stream_t *tx)
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

static inline bool tx_reserve(tx_stream_t *tx, uint32_t needed)
{
	if ((tx->idx + needed) >= TX_BUF_SIZE) {
		return tx_flush(tx);
	}

	return true;
}

static inline bool tx_write_u8(tx_stream_t *tx, uint8_t value)
{
	if (!tx_reserve(tx, 1)) {
		return false;
	}

	self.tx.buf[tx->idx++] = value;

	return true;
}

static bool tx_write_rle(tx_stream_t *tx, uint16_t sample, uint32_t run)
{
	while (run > 0) {

		uint32_t chunk = MIN(run, RLE_MAX_RUN);

		uint32_t needed = 1 + self.tx.bytes_per_dig_sample;

		if (!tx_reserve(tx, needed)) {
			return false;
		}

		self.tx.buf[tx->idx++] = (uint8_t)(RLE_BASE + (chunk - 1));

		if (self.tx.bytes_per_dig_sample >= 1) {
			self.tx.buf[tx->idx++] = (uint8_t)(0x80u | (sample & 0x7Fu));
		}

		if (self.tx.bytes_per_dig_sample >= 2) {
			self.tx.buf[tx->idx++] = (uint8_t)(0x80u | ((sample >> 7) & 0x7Fu));
		}

		run -= chunk;
	}

	return true;
}

static bool tx_write_analog_samples(tx_stream_t *tx, uint32_t start_sample, uint32_t count)
{
	struct ana_adc_module *adc = ana_adc_get_module();

	for (uint32_t s = 0; s < count; s++) {

		uint32_t sample_idx = start_sample + s;

		if (!tx_reserve(tx, active_analog_count)) {
			return false;
		}

		for (uint8_t ch = 0; ch < active_analog_count; ch++) {

			self.tx.buf[tx->idx++] =
				ana_adc_sigrok_byte(active_analog_channels[ch], sample_idx, adc);
		}
	}

	return true;
}

static bool ana_send_packet_channels(uint32_t chunk_samples, uint32_t *bytes_out)
{
	struct ana_module_system *channels = ana_channels_get_module();
	struct ana_adc_module *adc = ana_adc_get_module();

	const uint16_t *dig = channels->dma.dma_buffer;

	tx_stream_t tx = {
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

			uint16_t sample =
				(uint16_t)((dig[i] >> 4) & self.digital_mask);

			uint32_t needed =
				self.tx.bytes_per_dig_sample + active_analog_count;

			if (!tx_reserve(&tx, needed)) {
				return false;
			}

			/*
			 * Digital sample bytes
			 */
			if (self.tx.bytes_per_dig_sample >= 1) {
				self.tx.buf[tx.idx++] =
					(uint8_t)(0x80u | (sample & 0x7Fu));
			}

			if (self.tx.bytes_per_dig_sample >= 2) {
				self.tx.buf[tx.idx++] =
					(uint8_t)(0x80u | ((sample >> 7) & 0x7Fu));
			}

			/*
			 * Analog bytes
			 */
			for (uint8_t ch = 0; ch < active_analog_count; ch++) {

				self.tx.buf[tx.idx++] =
					ana_adc_sigrok_byte(
						active_analog_channels[ch],
						i,
						adc);
			}
		}

		if (!tx_flush(&tx)) {
			return false;
		}

		*bytes_out = tx.bytes_sent;

		return true;
	}

	uint16_t prev =
		(uint16_t)((dig[0] >> 4) & self.digital_mask);

	uint32_t run = 1;

	for (uint32_t i = 1; i < chunk_samples; i++) {

		uint16_t cur =
			(uint16_t)((dig[i] >> 4) & self.digital_mask);

		bool same = (cur == prev);

		if (same && run < RLE_MAX_RUN) {
			run++;
			continue;
		}

		if (!tx_write_rle(&tx, prev, run)) {
			return false;
		}

		prev = cur;
		run = 1;
	}

	if (!tx_write_rle(&tx, prev, run)) {
		return false;
	}

	if (!tx_flush(&tx)) {
		return false;
	}

	*bytes_out = tx.bytes_sent;

	return true;
}

void run_capture(bool continuous)
{
	struct ana_module_system *config = ana_channels_get_module();

	ana_channels_apply_trigger();

	ana_module_set_sample_rate(config);

	tx_init();
	ana_led_set_status(LED_STATUS_CAPTURING);

	do {
		uint32_t remaining = self.cfg.samples;
		uint32_t total_sent = 0;
		bool ok = true;

		while (remaining > 0 && ana_usb_is_connected()) {
			memset(self.tx.buf, 0, sizeof(self.tx.buf));

			uint32_t chunk =
				(remaining < CAPTURE_CHUNK_SIZE) ? remaining : CAPTURE_CHUNK_SIZE;

			self.cfg.samples = chunk;
			ana_module_set_sample_rate(config);

			memset(config->dma.dma_buffer, 0, chunk * sizeof(uint16_t));
			ana_capture_data_start(config);

			if (self.tx.active_analog_ch > 0) {
				ana_adc_capture_dma(chunk, self.analog_mask);
			}

			uint32_t chunk_bytes = 0;
			ok = ana_send_packet_channels(chunk, &chunk_bytes);
			if (!ok) {
				break;
			}

			total_sent += chunk_bytes;
			remaining -= chunk;
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

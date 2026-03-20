/*******************************************************************
 * @file sigrok_handler.c
 *
 * @brief Handles the Sigrok protocol communication over USB CDC.
 *
 * Protocol reference: https://github.com/pico-coder/sigrok-pico
 *
 * @author Vinicius Rafael Marques de Carvalho (vinicius.carvalho@edge.ufal.br)
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.3
 * @date 18/03/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#include "capture_data.h"
#include "channels.h"
#include "led.h"
#include "log.h"
#include "macros.h"
#include "sigrok_handler.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hardware/clocks.h>
#include <hardware/vreg.h>
#include <pico/time.h>
#include <tusb.h>

#define SIGROK_SAMPLE_RATE_MIN  5000U
#define SIGROK_SAMPLE_RATE_MAX  120000000U
#define SIGROK_SAMPLE_LIMIT_MAX 1024U

#define RLE_BASE    0x30U
#define RLE_MAX_RUN 32U

#define TX_BUF_SIZE 4096U

#define DIGITAL_MASK_DEFAULT 0x0FFF /**< First 12 digital channels */
#define ANALOG_MASK_DEFAULT  0x07   /**< All 3 analog channels */

/* ----------------------------------------------------------------
 * Identification string
 *   "SRPICO,A<a>D<d>,<ver>"
 *   A03  = 3 analog channels
 *   D16  = 16 digital channels
 *   02   = protocol version 2
 * ---------------------------------------------------------------- */
#define SIGROK_IDENT_STRING "SRPICO,A031D16,02"

static struct SIGROK_HANDLER {
	struct pulseview_sample cfg;
	struct {
		uint32_t bytes_per_dig_sample; /**< Bytes needed per digital sample (1 or 2) */
		uint32_t active_analog_ch;     /**< Number of enabled analog channels */
		uint32_t bytes_per_sample;     /**< Total bytes per combined sample */
		uint8_t buf[TX_BUF_SIZE];
	} tx;
	uint32_t sample_rate;
	uint32_t num_samples;
	uint16_t digital_mask;
	uint8_t analog_mask;
	uint8_t analog_channel;
	uint8_t digital_channel;
	uint8_t digital_bits_per_transfer;
	int8_t cmd_str_index;
	int8_t cmd_str[32];
	int8_t response[64];
	char *end_ptr;
} self = {
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
	.tx = {0},
	.cfg =
		{
			.sample_rate_hz = 5000,
			.samples = 1000,
		},
};

typedef struct {
	uint8_t command;
	void (*handler)(void);
} sigrok_command_t;

static void ana_send_response(const char *str)
{
	if (!tud_cdc_connected()) {
		log_debug("sigrok", "Cannot send response (disconnected): %s", str);
		return;
	}

	tud_cdc_write(str, strlen(str));
	tud_cdc_write_flush();
}

static bool ana_send_bytes(const uint8_t *buf, uint32_t len)
{
	uint32_t sent = 0;

	while (sent < len) {
		if (!tud_cdc_connected()) {
			return false;
		}

		uint32_t avail = tud_cdc_write_available();
		if (avail == 0) {
			tud_task();
			continue;
		}

		uint32_t chunk = (len - sent) < avail ? (len - sent) : avail;
		uint32_t written = tud_cdc_write(buf + sent, chunk);
		sent += written;

		if ((sent % 64 == 0) || (sent == len)) {
			tud_cdc_write_flush();
		} else {
			tud_task();
		}
	}
	return true;
}

/**
 * @brief Pre-compute per-capture transmission parameters.
 *
 * Must be called before ana_send_packet_channels() each capture.
 * Mirrors the tx_init() function from the reference implementation.
 */
static void tx_init(void)
{
	int highest_dig_bit = -1;
	for (int i = 15; i >= 0; i--) {
		if (self.digital_mask & (1u << i)) {
			highest_dig_bit = i;
			break;
		}
	}

	if (highest_dig_bit < 0) {
		self.tx.bytes_per_dig_sample = 0; /* No digital channels enabled */
	} else if (highest_dig_bit < 8) {
		self.tx.bytes_per_dig_sample = 1; /* ≤8 channels → 1 byte */
	} else {
		self.tx.bytes_per_dig_sample = 2; /* 9–16 channels → 2 bytes */
	}

	self.digital_bits_per_transfer = self.tx.bytes_per_dig_sample;

	self.tx.active_analog_ch = ana_capture_data_get_analog_channels_count(self.analog_mask);
	self.tx.bytes_per_sample = self.tx.bytes_per_dig_sample + self.tx.active_analog_ch;
}

static void rle_flush_run(uint8_t *out, uint32_t *idx, uint16_t sample, uint32_t run)
{
	uint32_t id = *idx;

	while (run > 0) {
		uint32_t chunk = MIN(run, RLE_MAX_RUN);

		/* Count token — high bit clear, in range 0x30–0x4F */
		out[id] = (uint8_t)(RLE_BASE + (chunk - 1));

		id++;

		if (self.tx.bytes_per_dig_sample >= 1) {
			out[id] = (uint8_t)(0x80 | (sample & 0x7F));

			id++;
		}

		if (self.tx.bytes_per_dig_sample >= 2) {
			out[id] = (uint8_t)(0x80 | ((sample >> 7) & 0x7F));

			id++;
		}

		run -= chunk;
	}

	*idx = id;
}

static bool ana_send_packet_channels(void)
{
	struct ana_module_system *ch = ana_channels_get_module();
	const uint16_t *dig_ptr = ch->dma.dma_buffer;

	uint32_t buf_idx = 0;
	uint32_t total_sent = 0;

	/* RLE state */
	uint16_t prev_sample = (uint16_t)((*dig_ptr) & self.digital_mask);
	uint32_t run_count = 1;

	/*
	 * Flush the TX buffer to USB if it is getting full.
	 * Worst case per flush: RLE_MAX_RUN token (1) + 2 sample bytes = 3 bytes.
	 * We flush when there is less than 64 bytes left to keep the buffer safe.
	 */
#define FLUSH_THRESHOLD 64U

	for (uint32_t i = 1; i < self.num_samples; i++) {
		uint16_t cur_sample = (uint16_t)(dig_ptr[i] & self.digital_mask);

		if (cur_sample == prev_sample && run_count < RLE_MAX_RUN) {
			run_count++;
		} else {
			rle_flush_run(self.tx.buf, &buf_idx, prev_sample, run_count);

			/* TODO: interleave analog samples here when ADC is ready
			 * for (uint32_t a = 0; a < tx.active_analog_ch; a++) {
			 *     self.tx_buf[buf_idx++] =
			 *         0x80 | ((ana_buffer[ana_idx++] >> 1) & 0x7F);
			 * }
			 */

			/* Flush TX buffer to USB if threshold reached */
			if (buf_idx + FLUSH_THRESHOLD > TX_BUF_SIZE) {
				if (!ana_send_bytes(self.tx.buf, buf_idx)) {
					return false; /* disconnected */
				}
				total_sent += buf_idx;
				buf_idx = 0;
			}

			prev_sample = cur_sample;
			run_count = 1;
		}
	}

	rle_flush_run(self.tx.buf, &buf_idx, prev_sample, run_count);

	if (buf_idx > 0) {
		if (!ana_send_bytes(self.tx.buf, buf_idx)) {
			return false;
		}
		total_sent += buf_idx;
	}

	char done_marker[32];
	snprintf(done_marker, sizeof(done_marker), "$%lu+", (unsigned long)total_sent);
	ana_send_response(done_marker);

	return true;
}

static void run_capture(bool continuous)
{
	bool ok;
	memset(self.response, 0, sizeof(char)*64);
	self.response[0] = '\0';

	struct ana_module_system *config = ana_channels_get_module();

	ana_module_set_sample_rate(config);

	tx_init();

	ana_led_set_status(LED_STATUS_CAPTURING);

	do {
		memset(config->dma.dma_buffer, 0, self.cfg.samples * sizeof(uint16_t) - 1);

		ana_capture_data_start(config);

		ok = ana_send_packet_channels();

		if (!ok) {
			ana_send_response("!!!");
			log_warn("sigrok", "Capture aborted: host disconnected");
			break;
		}

	} while (continuous && tud_cdc_connected());

	ana_led_set_status(LED_STATUS_CONNECTED);
}

static void handle_identify(void)
{
	ana_send_response(SIGROK_IDENT_STRING);
	ana_led_set_status(LED_STATUS_CONNECTED);
}

static void handle_set_sample_rate(void)
{
	uint32_t rate = (uint32_t)strtol((char *)&self.cmd_str[1], &self.end_ptr, 10);

	if (self.end_ptr == NULL || *(self.end_ptr) != '\0') {
		log_warn("sigrok", "Invalid sample rate: %s", &self.cmd_str[1]);
		return;
	}

	/* Clamp to supported range */
	if (rate < SIGROK_SAMPLE_RATE_MIN) {
		rate = SIGROK_SAMPLE_RATE_MIN;
	} else if (rate > SIGROK_SAMPLE_RATE_MAX) {

#if ENABLE_OVERCLOCKING
		vreg_set_voltage(VREG_VOLTAGE_1_25);
		sleep_ms(1);
		set_sys_clock_khz(250000, true);
#endif
		rate = SIGROK_SAMPLE_RATE_MAX;
	}

	self.cfg.sample_rate_hz = rate;
	log_inf("sigrok", "Sample rate set: %lu Hz", (unsigned long)self.cfg.sample_rate_hz);
}

static void handle_set_sample_limit(void)
{
	uint32_t limit = (uint32_t)strtol((char *)&self.cmd_str[1], &self.end_ptr, 10);

	if (self.end_ptr == NULL || *(self.end_ptr) != '\0') {
		log_warn("sigrok", "Invalid sample limit: %s", &self.cmd_str[1]);
		return;
	}

	if (limit > SIGROK_SAMPLE_LIMIT_MAX) {
		limit = SIGROK_SAMPLE_LIMIT_MAX;
	}
	if (limit == 0) {
		limit = 1;
	}

	self.cfg.samples = limit;
	log_inf("sigrok", "Sample limit set: %lu", (unsigned long)self.cfg.samples);
}

static void handle_get_analog_scale(void)
{
	int ch = (int)strtol((char *)&self.cmd_str[1], &self.end_ptr, 10);

	if (self.end_ptr == NULL || *(self.end_ptr) != '\0') {
		log_warn("sigrok", "Invalid analog scale channel");
		ana_send_response("ERR");
		return;
	}

	if (ch >= 0 && ch <= 2) {
		ana_send_response("25700x0");
	} else {
		ana_send_response("ERR");
	}
}

static void handle_set_analog_channel(void)
{
	int enable = self.cmd_str[1] - '0';
	int ch = (int)strtol((char *)&self.cmd_str[2], &self.end_ptr, 10);

	if (self.end_ptr == NULL || *self.end_ptr != '\0') {
		log_warn("sigrok", "Invalid analog channel: %s", &self.cmd_str[2]);
		return;
	}

	if (ch >= 0 && ch <= 2) {
		if (enable) {
			self.analog_mask |= (uint8_t)(1u << ch);
		} else {
			self.analog_mask &= (uint8_t)(~(1u << ch));
		}
		log_inf("sigrok", "Analog ch %d %s", ch, enable ? "enabled" : "disabled");
	}
}

static void handle_set_digital_channel(void)
{
	int enable = self.cmd_str[1] - '0';
	int ch = (int)strtol((char *)&self.cmd_str[2], &self.end_ptr, 10);

	if (self.end_ptr == NULL || *self.end_ptr != '\0') {
		log_warn("sigrok", "Invalid digital channel: %s", &self.cmd_str[2]);
		return;
	}

	if (ch >= 0 && ch <= 15) {
		if (enable) {
			self.digital_mask |= (uint16_t)(1u << ch);
		} else {
			self.digital_mask &= (uint16_t)(~(1u << ch));
		}
		log_inf("sigrok", "Digital ch %d %s", ch, enable ? "enabled" : "disabled");
	}
}

static void handle_fixed_capture(void)
{
	run_capture(false);
}

/**
 * @brief Handle the 'C' command — continuous capture.
 *
 * TODO: implement true streaming with double-buffering once
 *       the DMA chaining infrastructure is in place. @JoaoMatheusND
 */
static void handle_continuous_capture(void)
{
	run_capture(false);
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
};

#define SIGROK_COMMAND_COUNT (sizeof(sigrok_commands) / sizeof(sigrok_commands[0]))

void ana_sigrok_handle_init(void)
{
	self.cmd_str_index = 0;
	memset(self.cmd_str, 0, sizeof(self.cmd_str));
}

void ana_sigrok_handle_process_byte(uint8_t received_command)
{
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

	} else if (self.cmd_str_index < 31) {
		self.cmd_str[self.cmd_str_index] = (char)received_command;
		self.cmd_str_index++;
	} else {
		log_warn("sigrok", "Command buffer overflow, resetting");
		self.cmd_str_index = 0;
	}
}

struct pulseview_sample *ana_sigrok_get_sample_config()
{
	return &self.cfg;
}

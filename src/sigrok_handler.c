/*******************************************************************
 * @file sigrok_handler.c
 *
 * @brief Handles the Sigrok protocol communication over USB CDC.
 * @author Vinicius Rafael Marques de Carvalho (vinicius.carvalho@edge.ufal.br)
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.2
 * @date 20/02/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#include "capture_data.h"
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
#include <tusb.h>

#define DIGITAL_MASK_DEFAULT 0xFFFF
#define ANALOG_MASK_DEFAULT  0x07

/*
 * TODO: when the modules were full implemented, switch to a module struct to encapsulate the capture data
 * state @JoaoMatheusND
 */
static struct SIGROK_HANDLER {
	uint32_t sample_rate;
	uint32_t num_samples;
	uint16_t digital_mask;
	uint8_t analog_mask;
	uint8_t packet[1024];
	uint analog_channel;
	uint digital_channel;
	uint digital_bits_per_transfer;
	int cmd_str_index;
	char cmd_str[32];
	char response[64];
} self = {
	.sample_rate = 5000,
	.num_samples = 1024,
	.digital_mask = DIGITAL_MASK_DEFAULT,
	.analog_mask = ANALOG_MASK_DEFAULT,
	.packet = {0},
	.analog_channel = 0,
	.digital_channel = 0,
	.digital_bits_per_transfer = 2,
	.cmd_str = {0},
	.response = {0},
	.cmd_str_index = 0,
};

static char *end;

typedef struct {
	uint8_t command;
	void (*handler)(void);
} sigrok_command_t;

/**
 * @brief Send a response string over USB CDC
 *
 * @param str Response string to send
 */
static void ana_send_response(const char *str)
{
	if (!tud_cdc_connected()) {
		log_debug("ana_send_response", "Impossible to send response: %s", str);
		return;
	}

	tud_cdc_write(str, strlen(str));
	tud_cdc_write_flush();
}

/**
 * @brief Send mixed digital and analog signal data over USB CDC
 *
 */
static void ana_send_data_buffers(uint8_t *packet, uint32_t packet_index)
{
	uint32_t sent = 0;

	while (sent < packet_index) {

		if (!tud_cdc_connected()) {
			tud_task();
			continue;
		}

		uint32_t avail_usb = tud_cdc_write_available();
		if (avail_usb == 0) {
			tud_task();
			continue;
		}

		uint32_t remaining = packet_index - sent;
		uint32_t to_send = remaining < avail_usb ? remaining : avail_usb;

		uint32_t written = tud_cdc_write(&packet[sent], to_send);
		sent += written;

		if (sent % 64 == 0 || sent == packet_index) {
			tud_cdc_write_flush();
		} else {
			tud_task();
		}
	}
}

/**
 * @brief Updates digital settings based on the current digital mask
 *
 * @note If the selected channels are less than 7, use 1 byte per transfer
 *       If between 8 and 14, use 2 bytes per transfer.
 */
static void ana_update_digital_settings()
{
	int enabled_count = 0;
	for (int i = 0; i < 16; i++) {
		if ((self.digital_mask >> i) & 1) {
			enabled_count = i + 1;
		}
	}
	self.digital_bits_per_transfer = (enabled_count + 6) / 7;
	if (self.digital_bits_per_transfer == 0) {
		self.digital_bits_per_transfer = 1;
	}
}

/**
 * @brief Prepare and send a data packet combining digital and analog samples
 *
 * @note digital_values takes the first 16 bits sampled by PIO
 *       analog values takes 3 bytes per sample from the ADC
 */
static void ana_send_packet_channels(void)
{
	memset(self.packet, 0, sizeof(self.packet));
	uint32_t packet_index = 0;

	const uint16_t *dig_pointer = ana_capture_data_get_digital_capture_buffer();
	const uint8_t *ana_pointer = ana_capture_data_get_analog_capture_buffer();

	int active_analog_channels = ana_capture_data_get_analog_channels_count(self.analog_mask);
	uint32_t bytes_per_samples = self.digital_bits_per_transfer + active_analog_channels;
	uint32_t raw_digital_sample = *dig_pointer;

	ana_update_digital_settings();

	for (uint32_t i = 0; i < self.num_samples; i++, dig_pointer++) {
		raw_digital_sample = *dig_pointer;

		/*
		 * Maybe the packet fill before the samples loop ends, so we need to send the packet
		 * and start a new one to avoid overflow
		 */
		if (packet_index + bytes_per_samples >= sizeof(self.packet)) {
			ana_send_data_buffers(self.packet, packet_index);
			packet_index = 0;
		}

		for (int b = 0; b < self.digital_bits_per_transfer; b++) {
			self.packet[packet_index] =
				(uint8_t)(0x80 | ((raw_digital_sample >> (b * 7)) & 0x7F));
			packet_index++;
		}

		for (int j = 0; j < active_analog_channels; j++) {
			self.packet[packet_index] =
				(uint8_t)(0x80 | ((*ana_pointer++ >> 1) & 0x7F));
			packet_index++;
		}
	}

	if (packet_index > 0) {
		ana_send_data_buffers(self.packet, packet_index);
	}
}

static void handle_identify(void)
{
	ana_send_response("SRPICO,A031D16,02");
	ana_led_set_status(LED_STATUS_CONNECTED);
}

static void handle_set_sample_rate(void)
{
	self.sample_rate = strtol(&self.cmd_str[1], &end, 10);

	if (*end != '\0') {
		log_debug("sigrok_handle", "Invalid sample rate");
		return;
	}

	if (self.sample_rate < 5000) {
		self.sample_rate = 5000;
	} else if (self.sample_rate >= 150000000) {

#ifdef ENABLE_OVERCLOCKING
		vreg_set_voltage(VREG_VOLTAGE_1_25);
		sleep_ms(1);
		set_sys_clock_khz(250000000, true);
#endif

		self.sample_rate = 150000000;
	}
}

static void handle_set_sample_limit(void)
{
	self.num_samples = strtol(&self.cmd_str[1], &end, 10);

	if (*end != '\0') {
		log_debug("sigrok_handle", "Invalid sample limit");
	}

	if (self.num_samples > CAPTURE_MAX_SAMPLES) {
		self.num_samples = CAPTURE_MAX_SAMPLES;
	}
}

static void handle_get_analog_scale(void)
{
	self.analog_channel = strtol(&self.cmd_str[1], &end, 10);

	if (*end != '\0') {
		self.analog_channel = 0;
		log_debug("sigrok_handle", "Invalid analog channel");
		return;
	}

	if (self.analog_channel >= 0 && self.analog_channel <= 2) {
		ana_send_response("25700x0");
	} else {
		ana_send_response("ERR");
	}
}

static void handle_set_analog_channel(void)
{
	int is_channel_enable = self.cmd_str[1] - '0';
	self.analog_channel = strtol(&self.cmd_str[2], &end, 10);

	if (*end != '\0') {
		log_debug("sigrok_handle", "Invalid analog channel");
		return;
	}

	if (self.analog_channel >= 0 && self.analog_channel <= 2) {
		if (is_channel_enable) {
			self.analog_mask |= (1 << self.analog_channel);
		} else {
			self.analog_mask &= ~(1 << self.analog_channel);
		}
	}
}

static void handle_set_digital_channel(void)
{
	int is_digital_channel_enable = self.cmd_str[1] - '0';
	self.digital_channel = strtol(&self.cmd_str[2], &end, 10);

	if (*end != '\0') {
		log_debug("sigrok_handle", "Invalid digital channel");
		return;
	}

	if (self.digital_channel >= 0 && self.digital_channel <= 15) {
		if (is_digital_channel_enable) {
			self.digital_mask |= (1 << self.digital_channel);
		} else {
			self.digital_mask &= ~(1 << self.digital_channel);
		}
	}
}

static void handle_fixed_capture(void)
{
	self.response[0] = '\0';
	ana_led_set_status(LED_STATUS_CAPTURING);
	ana_capture_data_start(self.num_samples, self.sample_rate, self.analog_mask);
	ana_send_packet_channels();
	ana_led_set_status(LED_STATUS_CONNECTED);
}

static const sigrok_command_t sigrok_commands[] = {
	{SIGROK_CMD_IDENTIFY, handle_identify},
	{SIGROK_CMD_SET_SAMPLE_RATE, handle_set_sample_rate},
	{SIGROK_CMD_SET_SAMPLE_LIMIT, handle_set_sample_limit},
	{SIGROK_CMD_GET_ANALOG_SCALE, handle_get_analog_scale},
	{SIGROK_CMD_SET_ANALOG_CHANNEL, handle_set_analog_channel},
	{SIGROK_CMD_SET_DIGITAL_CHANNEL, handle_set_digital_channel},
	{SIGROK_CMD_FIXED_CAPTURE, handle_fixed_capture},
};

void ana_sigrok_handle_init(void)
{
	self.cmd_str_index = 0;
	memset(self.cmd_str, 0, sizeof(self.cmd_str));
}

void ana_sigrok_handle_process_byte(uint8_t received_command)
{
	memset(&self.response, 0, sizeof(self.response));
	self.response[0] = '\0';

	if (received_command == '*') {
		ana_sigrok_handle_init();
		ana_led_set_status(LED_STATUS_OFF);
		return;
	}

	if (received_command == '\r' || received_command == '\n') {
		self.cmd_str[self.cmd_str_index] = '\0';

		strcpy(self.response, "*");

		/*
		 * Since there are few commands, the time to find and process the correct command is
		 * fast.
		 */
		for (size_t i = 0; i < sizeof(sigrok_commands) / sizeof(sigrok_command_t); i++) {
			if (self.cmd_str[0] == sigrok_commands[i].command) {
				sigrok_commands[i].handler();
				break;
			}
		}

		if (self.response[0] != '\0') {
			ana_send_response(self.response);
		}

		self.cmd_str_index = 0;
	} else {
		if (self.cmd_str_index < 31) {
			self.cmd_str[self.cmd_str_index] = (char)received_command;
			self.cmd_str_index += 1;
		} else {
			self.cmd_str_index = 0;
		}
	}
}

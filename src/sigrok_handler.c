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

static uint32_t sample_rate = 5000;
static uint32_t num_samples = 1024;
static uint16_t digital_mask = 0xFFFF;
static uint8_t analog_mask = 0x07;
static uint analog_channel;
static uint digital_channel;
static uint digital_bits_per_transfer = 2;

static char cmd_str[32];
static char response[64];
static char *end;
static int cmd_str_index = 0;

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
	if (tud_cdc_connected()) {
		tud_cdc_write(str, strlen(str));
		tud_cdc_write_flush();
	}
}

/**
 * @brief Send mixed digital and analog signal data over USB CDC
 *
 */
static void ana_send_data_buffers(uint8_t *packet, uint32_t packet_index)
{
	uint32_t sent = 0;
	while (sent < packet_index) {
		uint32_t avail_usb = tud_cdc_write_available();
		if (avail_usb > 0) {
			uint32_t count = (packet_index - sent) < avail_usb ? (packet_index - sent)
									   : avail_usb;
			sent += tud_cdc_write(&packet[sent], count);
			(sent % 64 == 0 || sent == packet_index) ? tud_cdc_write_flush()
								 : tud_task();
		} else {
			tud_task();
		}
	}
	packet_index = 0;
}

/**
 * @brief Updates digital settings based on the current digital mask
 *
 * @note If the selected channels are less than 7, use 1 byte per transfer
 *       If between 8 and 14, use 2 bytes per transfer.
 */
void ana_update_digital_settings()
{
	int enabled_count = 0;
	for (int i = 0; i < 16; i++) {
		if ((digital_mask >> i) & 1) {
			enabled_count = i + 1;
		}
	}
	digital_bits_per_transfer = (enabled_count + 6) / 7;
	if (digital_bits_per_transfer == 0) {
		digital_bits_per_transfer = 1;
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
	uint8_t packet[1024];
	uint32_t packet_index = 0;

	const uint16_t *dig_pointer = ana_get_digital_capture_buffer();
	const uint8_t *ana_pointer = ana_get_analog_capture_buffer();

	int active_analog_channels = ana_get_analog_channels_count(analog_mask);
	ana_update_digital_settings();

	for (uint32_t i = 0; i < num_samples; i++) {
		uint16_t raw_digital_sample = *dig_pointer;
		dig_pointer += 1;

		uint16_t digital_val = raw_digital_sample;

		for (int b = 0; b < digital_bits_per_transfer; b++) {
			packet[packet_index++] =
				(uint8_t)(0x80 | ((digital_val >> (b * 7)) & 0x7F));
		}

		for (int j = 0; j < active_analog_channels; j++) {
			packet[packet_index++] = (uint8_t)(0x80 | ((*ana_pointer++ >> 1) & 0x7F));
		}

		if (packet_index >= 512) {
			ana_send_data_buffers(packet, packet_index);
			packet_index = 0;
		}
	}
	if (packet_index > 0) {
		ana_send_data_buffers(packet, packet_index);
	}
}

static void handle_identify(void)
{
	ana_send_response("SRPICO,A031D16,02");
	ana_led_set_status(LED_STATUS_CONNECTED);
}

static void handle_set_sample_rate(void)
{
	sample_rate = strtol(&cmd_str[1], &end, 10);

	if (*end != '\0') {
		log_debug("sigrok_handle", "Invalid sample rate");
		return;
	}

	if (sample_rate < 5000) {
		sample_rate = 5000;
	} else if (sample_rate >= 150000000) {

#ifdef ENABLE_OVERCLOCKING
		vreg_set_voltage(VREG_VOLTAGE_1_25);
		sleep_ms(1);
		set_sys_clock_khz(250000000, true);
#endif

		sample_rate = 150000000;
	}
}

static void handle_set_sample_limit(void)
{
	num_samples = strtol(&cmd_str[1], &end, 10);

	if (*end != '\0') {
		log_debug("sigrok_handle", "Invalid sample limit");
	}

	if (num_samples > CAPTURE_BUFFER_SIZE) {
		num_samples = CAPTURE_BUFFER_SIZE;
	}
}

static void handle_get_analog_scale(void)
{
	analog_channel = strtol(&cmd_str[1], &end, 10);

	if (*end != '\0') {
		analog_channel = 0;
		log_debug("sigrok_handle", "Invalid analog channel");
		return;
	}

	if (analog_channel >= 0 && analog_channel <= 2) {
		ana_send_response("25700x0");
	} else {
		ana_send_response("ERR");
	}
}

static void handle_set_analog_channel(void)
{
	int is_channel_enable = cmd_str[1] - '0';
	analog_channel = strtol(&cmd_str[2], &end, 10);

	if (*end != '\0') {
		log_debug("sigrok_handle", "Invalid analog channel");
		return;
	}

	if (analog_channel >= 0 && analog_channel <= 2) {
		if (is_channel_enable) {
			analog_mask |= (1 << analog_channel);
		} else {
			analog_mask &= ~(1 << analog_channel);
		}
	}
}

static void handle_set_digital_channel(void)
{
	int is_digital_channel_enable = cmd_str[1] - '0';
	digital_channel = strtol(&cmd_str[2], &end, 10);

	if (*end != '\0') {
		log_debug("sigrok_handle", "Invalid digital channel");
		return;
	}

	if (digital_channel >= 0 && digital_channel <= 15) {
		if (is_digital_channel_enable) {
			digital_mask |= (1 << digital_channel);
		} else {
			digital_mask &= ~(1 << digital_channel);
		}
	}
}

static void handle_fixed_capture(void)
{
	response[0] = '\0';
	ana_led_set_status(LED_STATUS_CAPTURING);
	ana_capture_data(num_samples, sample_rate, analog_mask);
	ana_send_packet_channels();
	ana_led_set_status(LED_STATUS_CONNECTED);
}

static const sigrok_command_t sigrok_commands[] = {
	{IDENTIFY_CMD, handle_identify},
	{SET_SAMPLE_RATE_CMD, handle_set_sample_rate},
	{SET_SAMPLE_LIMIT_CMD, handle_set_sample_limit},
	{GET_ANALOG_SCALE_CMD, handle_get_analog_scale},
	{SET_ANALOG_CHANNEL_CMD, handle_set_analog_channel},
	{SET_DIGITAL_CHANNEL_CMD, handle_set_digital_channel},
	{FIXED_CAPTURE_CMD, handle_fixed_capture},
};

void sigrok_init(void)
{
	cmd_str_index = 0;
	memset(cmd_str, 0, sizeof(cmd_str));
}

void sigrok_process_byte(uint8_t received_command)
{
	memset(&response, 0, sizeof(response));
	response[0] = '\0';

	if (received_command == '*') {
		sigrok_init();
		ana_led_set_status(LED_STATUS_OFF);
		return;
	}

	if (received_command == '\r' || received_command == '\n') {
		cmd_str[cmd_str_index] = '\0';

		strcpy(response, "*");

		/*
		 * Since there are few commands, the time to find and process the correct command is
		 * fast.
		 */
		for (size_t i = 0; i < sizeof(sigrok_commands) / sizeof(sigrok_command_t); i++) {
			if (cmd_str[0] == sigrok_commands[i].command) {
				sigrok_commands[i].handler();
				break;
			}
		}

		if (response[0] != '\0') {
			ana_send_response(response);
		}

		cmd_str_index = 0;
	} else {
		if (cmd_str_index < 31) {
			cmd_str[cmd_str_index] = (char)received_command;
			cmd_str_index += 1;
		} else {
			cmd_str_index = 0;
		}
	}
}

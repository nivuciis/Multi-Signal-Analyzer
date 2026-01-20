/*******************************************************************
 * @file sigrok_handler.c
 *
 * @brief Handles the Sigrok protocol communication over USB CDC.
 * @author Vinicius Rafael Marques de Carvalho (vinicius.carvalho@edge.ufal.br)
 * @version 0.1
 * @date 15/01/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#include "capture_data.h"
#include "led.h"
#include "macros.h"
#include "sigrok_handler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hardware/clocks.h>
#include <hardware/vreg.h>
#include <tusb.h>

static uint32_t sample_rate = 5000;
static uint32_t num_samples = 1024;

static char cmd_str[32];
static int cmd_str_pointer = 0;

extern uint32_t digital_capture_buffer[];
extern uint8_t analog_capture_buffer[];

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
			sent % 64 == 0 ? tud_cdc_write_flush() : tud_task();
		} else {
			tud_task();
		}
	}
	packet_index = 0;
}

/**
 * @brief Prepare and send a data packet combining digital and analog samples
 *
 * @note digital_values takes the first 16 bits of the 32 bits sampled by PIO
 *       analog values takes 3 bytes per sample from the analog buffer
 */
static void ana_send_packet(void)
{
	uint8_t packet[256];
	uint32_t packet_index = 0;

	const uint32_t *dig_pointer = digital_capture_buffer;
	const uint8_t *ana_pointer = analog_capture_buffer;

	for (uint32_t i = 0; i < num_samples; i++) {
		uint32_t raw_digital_sample = *dig_pointer;
		dig_pointer += 1;
		uint16_t digital_value = (uint16_t)(raw_digital_sample >> 16);

		packet[packet_index++] = 0x80 | (digital_value & 0x7F);
		packet[packet_index++] = 0x80 | ((digital_value >> 7) & 0x7F);
		packet[packet_index++] = 0x80 | ((digital_value >> 14) & 0x03);

		for (int j = 0; j < 3; j++) {
			packet[packet_index] = 0x80 | (*ana_pointer >> 1);
			ana_pointer += 1;
			packet_index += 1;
		}

		if (packet_index >= 200) {
			ana_send_data_buffers(packet, packet_index);
			packet_index = 0;
		}
	}
	if (packet_index > 0) {
		ana_send_data_buffers(packet, packet_index);
	}
}

void sigrok_init(void)
{
	cmd_str_pointer = 0;
	memset(cmd_str, 0, sizeof(cmd_str));
}

void sigrok_process_byte(uint8_t received_command)
{
	char response[64];
	response[0] = '\0';

	if (received_command == '*') {
		sigrok_init();
		ana_led_set_status(LED_STATUS_OFF);
		return;
	}

	if (received_command == '\r' || received_command == '\n') {
		cmd_str[cmd_str_pointer] = '\0';

		strcpy(response, "*");

		switch (cmd_str[0]) {
		case IDENTIFY_CMD:
			tud_cdc_write_str("SRPICO,A031D16,02");
			tud_cdc_write_flush();
			ana_led_set_status(LED_STATUS_CONNECTED);
			break;

		case SET_SAMPLE_RATE_CMD:
			sample_rate = atol(&cmd_str[1]);
			if (sample_rate < 5000) {
				sample_rate = 5000;
			} else if (sample_rate >= 200000000) {
#ifdef ENABLE_OVERCLOCKING
				vreg_set_voltage(VREG_VOLTAGE_1_25);
				sleep_ms(1);
				set_sys_clock_khz(250000, true);
#endif
				sample_rate = 200000000;
			}
			break;

		case SET_SAMPLE_LIMIT_CMD:
			num_samples = atol(&cmd_str[1]);
			if (num_samples > CAPTURE_BUFFER_SIZE) {
				num_samples = CAPTURE_BUFFER_SIZE;
			}
			break;

		case GET_ANALOG_SCALE_CMD:
			snprintf(response, sizeof(response), "25700x0");
			break;

		case ENABLE_ANALOG_CHANNEL_CMD:
			break;

		case ENABLE_DIGITAL_CHANNEL_CMD:
			break;

		case FIXED_CAPTURE_CMD:
			response[0] = 0;
			ana_led_set_status(LED_STATUS_CAPTURING);
			ana_capture_data(num_samples, sample_rate, NULL);
			ana_send_packet();
			ana_led_set_status(LED_STATUS_CONNECTED);
			break;

		default:
			break;
		}

		if (response[0] != 0) {
			ana_send_response(response);
		}

		cmd_str_pointer = 0;
	} else {
		if (cmd_str_pointer < 31) {
			cmd_str[cmd_str_pointer] = (char)received_command;
			cmd_str_pointer += 1;
		} else {
			cmd_str_pointer = 0;
		}
	}
}

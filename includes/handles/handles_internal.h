/*******************************************************************
 * @file handles_internal.h
 *
 * @brief Internal header shared between sigrok_handler.c and handle files.
 *
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 12/05/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#ifndef HANDLES_INTERNAL_H
#define HANDLES_INTERNAL_H

#include "adc.h"
#include "capture_data.h"
#include "channels.h"
#include "led.h"
#include "log.h"
#include "macros.h"
#include "sigrok_handler.h"
#include "usb_util.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TX_BUF_SIZE 4096U

#define SIGROK_SAMPLE_RATE_MIN  5000U
#define SIGROK_SAMPLE_RATE_MAX  120000000U
#define SIGROK_SAMPLE_LIMIT_MAX 1000000U

#if PICO_DEFAULT_ADC_VOLTAGE_DIVIDER
#define ADC_MV_FULL_SCALE   13200.0
#define SIGROK_ANALOG_SCALE "103125x0"
#else
#define ADC_MV_FULL_SCALE   3300.0
#define SIGROK_ANALOG_SCALE "25700x0"
#endif

#define DIGITAL_MASK_DEFAULT 0x0FFF
#define ANALOG_MASK_DEFAULT  0x00

struct SIGROK_HANDLER {
	struct pulseview_sample_config cfg;
	struct {
		uint32_t bytes_per_dig_sample;
		uint32_t active_analog_ch;
		uint32_t bytes_per_sample;
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
	struct sigrok_trigger trigger_config;
	bool last_was_cr;
};

extern struct SIGROK_HANDLER self;

void ana_send_response(const char *str);
void run_capture(bool continuous);

#endif /* HANDLES_INTERNAL_H */

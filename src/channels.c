/*******************************************************************
 * @file channels.c
 *
 * @brief Channel implementation program to digital channels
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 09/03/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/
#include "capture.pio.h"
#include "channels.h"
#include "module.h"
#include "debug.h"

#include <stdint.h>

#include <hardware/dma.h>
#include <hardware/pio.h>

#define MODULE_NAME "Channels"

static uint16_t digital_channel_buffer[MAX_SAMPLES_FOR_DEBUG];

struct ana_module_system channels = {
	.module =
		{
			.name = MODULE_NAME,
			.pin_base = PICO_DEFAULT_CHANNELS_PIN_BASE,
			.pin_count = PICO_DEFAULT_CHANNELS_PIN_COUNT,
			.samples = 1,
			.mask = 0xFFF0, /**< Default mask to select the first 12 bits/channels */
			.sample_rate_hz = 1,
		},
	.pio = {0},
	.dma = {0},
};

void ana_channels_init(PIO pio)
{
	channels.pio.instance = pio;
	channels.pio.pio_program = &capture_prog_program;
	channels.pio.get_default_cfg_func =
		(pio_sm_config(*)(uint8_t))capture_prog_program_get_default_config;

	channels.dma.dma_buffer = digital_channel_buffer;

	ana_module_pio_init(&channels);
	ana_module_dma_init(&channels);

	return;
}

struct ana_module_system *ana_channels_get_module(void)
{
	return &channels;
}
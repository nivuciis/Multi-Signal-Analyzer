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
#include "debug.h"
#include "module.h"
#include "handles/sigrok_handler.h"

#include <hardware/pio.h>

#define MODULE_NAME "Channels"

static uint16_t digital_channel_buffer[1024];

struct ana_module_system channels = {
	.module =
		{
			.name = MODULE_NAME,
			.pin_base = PICO_DEFAULT_CHANNELS_PIN_BASE,
			.pin_count = PICO_DEFAULT_CHANNELS_PIN_COUNT,
			.mask = 0xFFF0, /**< Default mask to select the first 12 bits/channels */
		},
	.pio = {0},
	.dma = {0},
};

void ana_channels_init(PIO pio)
{
	channels.pio.instance = pio;
	channels.pio.pio_program = &capture_prog_simple_program;
	channels.pio.get_default_cfg_func =
		(pio_sm_config(*)(uint8_t))capture_prog_simple_program_get_default_config;
	channels.pio.jmp_pin = 0xFF;

	channels.dma.dma_buffer = digital_channel_buffer;

	ana_module_pio_init(&channels);
	ana_module_dma_init(&channels);

	return;
}

struct ana_module_system *ana_channels_get_module(void)
{
	return &channels;
}

void ana_channels_apply_trigger(void)
{
	struct sigrok_trigger *trigger = ana_sigrok_get_trigger();

	const pio_program_t *prog = &capture_prog_simple_program;
	pio_sm_config (*cfg_func)(uint8_t) =
		(pio_sm_config(*)(uint8_t))capture_prog_simple_program_get_default_config;
	uint8_t jmp_pin = 0xFF; /* sentinel: no jmp_pin (matches ana_module_pio_init check) */

	int trig_ch = -1;
	enum ana_trigger_type trig_type = ANA_TRIGGER_EDGE_RISE;

	for (int i = 0; i < PICO_DEFAULT_CHANNELS_PIN_COUNT; i++) {
		if (trigger->trigger_mask & (uint16_t)(1u << i)) {
			trig_ch = i;
			trig_type = trigger->trigger_type[i];
			break;
		}
	}

	if (trig_ch >= 0) {
		jmp_pin = (uint8_t)(PICO_DEFAULT_CHANNELS_PIN_BASE + trig_ch);

		switch (trig_type) {
		case ANA_TRIGGER_EDGE_RISE:
			prog = &capture_prog_trigger_rise_program;
			cfg_func = (pio_sm_config(*)(
				uint8_t))capture_prog_trigger_rise_program_get_default_config;
			break;
		case ANA_TRIGGER_EDGE_FALL:
			prog = &capture_prog_trigger_fall_program;
			cfg_func = (pio_sm_config(*)(
				uint8_t))capture_prog_trigger_fall_program_get_default_config;
			break;
		case ANA_TRIGGER_EDGE_BOTH:
			prog = &capture_prog_trigger_both_program;
			cfg_func = (pio_sm_config(*)(
				uint8_t))capture_prog_trigger_both_program_get_default_config;
			break;
		case ANA_TRIGGER_LEVEL_LOW:
			prog = &capture_prog_trigger_low_level_program;
			cfg_func = (pio_sm_config(*)(
				uint8_t))capture_prog_trigger_low_level_program_get_default_config;
			break;
		case ANA_TRIGGER_LEVEL_HIGH:
			prog = &capture_prog_trigger_high_level_program;
			cfg_func = (pio_sm_config(*)(
				uint8_t))capture_prog_trigger_high_level_program_get_default_config;
			break;
		default:
			jmp_pin = 0xFF;
			prog = &capture_prog_simple_program;
			cfg_func = (pio_sm_config(*)(uint8_t))capture_prog_simple_program_get_default_config;
			break;
		}
	}

	ana_module_pio_reload(&channels, prog, cfg_func, jmp_pin);
}

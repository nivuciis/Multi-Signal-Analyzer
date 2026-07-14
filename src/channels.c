/*******************************************************************
 * @file channels.c
 *
 * @brief Channel implementation program to digital channels
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.2
 * @date 09/03/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/
#include "channels.h"
#include "debug.h"
#include "module.h"
#include "handles/sigrok_handler.h"

#define MODULE_NAME "Channels"

static uint16_t digital_channel_buffer_a[BUFFER_SIZE];
static uint16_t digital_channel_buffer_b[BUFFER_SIZE];

struct ana_module_system channels = {
	.module =
		{
			.name = MODULE_NAME,
			.pin_base = PICO_DEFAULT_CHANNELS_PIN_BASE,
			.pin_count = PICO_DEFAULT_CHANNELS_PIN_COUNT,
			.mask = 0x0FFF, /**< Channels 0-11 (bits 0-11) */
		},
	.capture = {0},
	.trigger = {0},
};

void ana_channels_init(void)
{
	channels.capture.buffer = digital_channel_buffer_a;

	ana_module_gpio_init(&channels);

	return;
}

struct ana_module_system *ana_channels_get_module(void)
{
	return &channels;
}

uint16_t *ana_channels_get_alt_buffer(void)
{
	return (channels.capture.buffer == digital_channel_buffer_a)
		       ? digital_channel_buffer_b
		       : digital_channel_buffer_a;
}

void ana_channels_load_simple(void)
{
	ana_module_clear_trigger(&channels);
}

void ana_channels_apply_trigger(void)
{
	struct sigrok_trigger *trigger = ana_sigrok_get_trigger();

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
		ana_module_set_trigger(&channels,
				       (uint8_t)(PICO_DEFAULT_CHANNELS_PIN_BASE + trig_ch),
				       trig_type);
	} else {
		ana_module_clear_trigger(&channels);
	}
}

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

/* 2048-byte alignment required by the ping-pong DMA write-address ring
 * (see ana_module_pp_configure). */
static uint16_t digital_channel_buffer_a[BUFFER_SIZE] __attribute__((aligned(2*BUFFER_SIZE)));
static uint16_t digital_channel_buffer_b[BUFFER_SIZE] __attribute__((aligned(2*BUFFER_SIZE)));

struct ana_module_system channels = {
	.module =
		{
			.name = MODULE_NAME,
			.pin_base = PICO_DEFAULT_CHANNELS_PIN_BASE,
			.pin_count = PICO_DEFAULT_CHANNELS_PIN_COUNT,
			.mask = 0x0FFF, /**< Channels 0-11 (bits 0-11) */
		},
	.pio = {0},
	.dma = {0},
};

void ana_channels_init(PIO pio)
{
	channels.pio.instance = pio;
	channels.pio.programs = (struct ana_module_programs){
		.simple = { &capture_prog_simple_program,
			    (pio_sm_config(*)(uint8_t))
				    capture_prog_simple_program_get_default_config },
		.trigger = {
			[ANA_TRIGGER_EDGE_RISE] = { &capture_prog_trigger_rise_program,
						    (pio_sm_config(*)(uint8_t))
							    capture_prog_trigger_rise_program_get_default_config },
			[ANA_TRIGGER_EDGE_FALL] = { &capture_prog_trigger_fall_program,
						    (pio_sm_config(*)(uint8_t))
							    capture_prog_trigger_fall_program_get_default_config },
			[ANA_TRIGGER_EDGE_BOTH] = { &capture_prog_trigger_both_program,
						    (pio_sm_config(*)(uint8_t))
							    capture_prog_trigger_both_program_get_default_config },
			[ANA_TRIGGER_LEVEL_LOW] = { &capture_prog_trigger_low_level_program,
						    (pio_sm_config(*)(uint8_t))
							    capture_prog_trigger_low_level_program_get_default_config },
			[ANA_TRIGGER_LEVEL_HIGH] = { &capture_prog_trigger_high_level_program,
						     (pio_sm_config(*)(uint8_t))
							     capture_prog_trigger_high_level_program_get_default_config },
		},
	};
	channels.pio.pio_program = channels.pio.programs.simple.program;
	channels.pio.get_default_cfg_func = channels.pio.programs.simple.get_default_cfg_func;
	channels.pio.jmp_pin = ANA_NO_JMP_PIN;

	channels.dma.dma_buffer = digital_channel_buffer_a;

	ana_module_pio_init(&channels);
	ana_module_dma_init(&channels);
	ana_module_pingpong_init(&channels, digital_channel_buffer_a, digital_channel_buffer_b,
				 BUFFER_SIZE);

	return;
}

struct ana_module_system *ana_channels_get_module(void)
{
	return &channels;
}

uint16_t *ana_channels_get_alt_buffer(void)
{
	return (channels.dma.dma_buffer == digital_channel_buffer_a)
		       ? digital_channel_buffer_b
		       : digital_channel_buffer_a;
}

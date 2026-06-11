/*******************************************************************
 * @file rs485.c
 *
 * @brief RS485 channel implementation for the Multi-Signal Analyzer project
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 25/05/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/
#include "rs485.pio.h"
#include "rs485.h"
#include "debug.h"
#include "module.h"
#include "handles/sigrok_handler.h"

#include <hardware/pio.h>

#define MODULE_NAME "RS485"

static uint16_t rs485_buffer_a[1024];
static uint16_t rs485_buffer_b[1024];

struct ana_module_system rs485 = {
	.module = {
		.name      = MODULE_NAME,
		.pin_base  = PICO_DEFAULT_RS485_PIN_BASE,
		.pin_count = PICO_DEFAULT_RS485_PIN_COUNT,
		.mask      = 0x0001,
	},
	.pio = {0},
	.dma = {0},
};

void ana_rs485_init(PIO pio)
{
	rs485.pio.instance = pio;
	rs485.pio.pio_program = &capture_rs485_simple_program;
	rs485.pio.get_default_cfg_func =
		(pio_sm_config (*)(uint8_t))capture_rs485_simple_program_get_default_config;
	rs485.pio.jmp_pin = 0xFF;

	rs485.dma.dma_buffer = rs485_buffer_a;

	ana_module_pio_init(&rs485);
	ana_module_dma_init(&rs485);
	ana_module_pingpong_init(&rs485, rs485_buffer_a, rs485_buffer_b, 1024);
}

struct ana_module_system *ana_rs485_get_module(void)
{
	return &rs485;
}

uint16_t *ana_rs485_get_alt_buffer(void)
{
	return (rs485.dma.dma_buffer == rs485_buffer_a) ? rs485_buffer_b : rs485_buffer_a;
}

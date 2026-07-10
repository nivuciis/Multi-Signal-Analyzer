/*******************************************************************
 * @file rs232.c
 *
 * @brief RS232 channel implementation for the Multi-Signal Analyzer project
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 15/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/
#include "rs232.pio.h"
#include "rs232.h"
#include "debug.h"
#include "module.h"
#include "handles/sigrok_handler.h"

#include <hardware/pio.h>

#define MODULE_NAME "RS232"

/* 2048-byte alignment required by the ping-pong DMA write-address ring
 * (see ana_module_pp_configure). */
static uint16_t rs232_buffer_a[BUFFER_SIZE] __attribute__((aligned(2 * BUFFER_SIZE)));
static uint16_t rs232_buffer_b[BUFFER_SIZE] __attribute__((aligned(2 * BUFFER_SIZE)));

struct ana_module_system rs232 = {
	.module = {
		.name      = MODULE_NAME,
		.pin_base  = PICO_DEFAULT_RS232_PIN_BASE,
		.pin_count = PICO_DEFAULT_RS232_PIN_COUNT,
		.mask      = 0x0003,
	},
	.pio = {0},
	.dma = {0},
};

void ana_rs232_init(PIO pio)
{
	rs232.pio.instance = pio;
	rs232.pio.pio_program = &capture_rs232_simple_program;
	rs232.pio.get_default_cfg_func =
		(pio_sm_config (*)(uint8_t))capture_rs232_simple_program_get_default_config;
	rs232.pio.jmp_pin = 0xFF;

	rs232.dma.dma_buffer = rs232_buffer_a;

	ana_module_pio_init(&rs232);
	ana_module_dma_init(&rs232);
	ana_module_pingpong_init(&rs232, rs232_buffer_a, rs232_buffer_b, BUFFER_SIZE);
}

struct ana_module_system *ana_rs232_get_module(void)
{
	return &rs232;
}

uint16_t *ana_rs232_get_alt_buffer(void)
{
	return (rs232.dma.dma_buffer == rs232_buffer_a) ? rs232_buffer_b : rs232_buffer_a;
}

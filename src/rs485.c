/*******************************************************************
 * @file rs485.c
 *
 * @brief RS485 channel implementation for the Multi-Signal Analyzer project
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.2
 * @date 25/05/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/
#include "rs485.h"
#include "debug.h"
#include "module.h"
#include "handles/sigrok_handler.h"

#define MODULE_NAME "RS485"

static uint16_t rs485_buffer_a[BUFFER_SIZE];
static uint16_t rs485_buffer_b[BUFFER_SIZE];

struct ana_module_system rs485 = {
	.module = {
		.name      = MODULE_NAME,
		.pin_base  = PICO_DEFAULT_RS485_PIN_BASE,
		.pin_count = PICO_DEFAULT_RS485_PIN_COUNT,
		.mask      = 0x0001,
	},
	.capture = {0},
	.trigger = {0},
};

void ana_rs485_init(void)
{
	rs485.capture.buffer = rs485_buffer_a;

	ana_module_gpio_init(&rs485);
}

struct ana_module_system *ana_rs485_get_module(void)
{
	return &rs485;
}

uint16_t *ana_rs485_get_alt_buffer(void)
{
	return (rs485.capture.buffer == rs485_buffer_a) ? rs485_buffer_b : rs485_buffer_a;
}

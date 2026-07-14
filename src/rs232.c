/*******************************************************************
 * @file rs232.c
 *
 * @brief RS232 channel implementation for the Multi-Signal Analyzer project
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.2
 * @date 15/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/
#include "rs232.h"
#include "debug.h"
#include "module.h"
#include "handles/sigrok_handler.h"

#define MODULE_NAME "RS232"

static uint16_t rs232_buffer_a[BUFFER_SIZE];
static uint16_t rs232_buffer_b[BUFFER_SIZE];

struct ana_module_system rs232 = {
	.module = {
		.name      = MODULE_NAME,
		.pin_base  = PICO_DEFAULT_RS232_PIN_BASE,
		.pin_count = PICO_DEFAULT_RS232_PIN_COUNT,
		.mask      = 0x0003,
	},
	.capture = {0},
	.trigger = {0},
};

void ana_rs232_init(void)
{
	rs232.capture.buffer = rs232_buffer_a;

	ana_module_gpio_init(&rs232);
}

struct ana_module_system *ana_rs232_get_module(void)
{
	return &rs232;
}

uint16_t *ana_rs232_get_alt_buffer(void)
{
	return (rs232.capture.buffer == rs232_buffer_a) ? rs232_buffer_b : rs232_buffer_a;
}
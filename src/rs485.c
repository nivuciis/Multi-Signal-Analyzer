/*******************************************************************
 * @file rs485.c
 *
 * @brief RS485 communication tests
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 07/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/
#include "bring_up_rs485.pio.h"
#include "config_pio.h"
#include "rs485.h"
#include "log.h"

#include <stdio.h>
#include <string.h>

#define RS485_SAMPLES       256
#define RS485_GPIO_PIN_BASE 31
#define RS485_PIN_COUNT     1
#define RS485_MODULE_NAME   "RS485"

static struct ana_config_system rs485;
static uint16_t dma_rs485_buffer[RS485_SAMPLES];

static void __not_in_flash_func(rs485_capture_finished)(void)
{
	uint32_t ints = dma_hw->ints0;
	dma_hw->ints0 = ints;

	if (ints & (1u << rs485.dma.instance)) {
		dma_channel_acknowledge_irq0(rs485.dma.instance);

		/**
		 * @note Never restart the DMA here if using wait_for_finish_blocking, cus it'll
		 * cause an infinite loop.
		 */

		rs485.dma.has_complete = true;

		pio_sm_set_enabled(rs485.pio.instance, rs485.pio.sm, false);

		log_inf(RS485_MODULE_NAME, "RS485 DMA IRQ triggered");
	}

	/**
	 * @note If use the dma_chan1 set the config bellow as  above
	 * if (ints & (1u << rs485.dma.instance)) { ... }
	 *
	 */
}

void ana_rs485_init(void)
{
	log_inf(RS485_MODULE_NAME, "Initializing...");

	rs485.pio.instance = pio0;
	rs485.pio.pio_program = &bring_up_rs485_program;
	rs485.pio.get_default_cfg_func = bring_up_rs485_program_get_default_config;

	/**
	 * @note To use wait_for_finish_blocking you can't use a callback
	 *
	 * use NULL for blocking mode
	 */

	/**< rs485_config.dma_callback = rs485_capture_finished;  */

	rs485.dma.callback = NULL; 

	rs485.dma.dma_buffer = dma_rs485_buffer;
	rs485.module.samples = RS485_SAMPLES;
	rs485.module.pin_base = PICO_DEFAULT_RS485_PIN_BASE;
	rs485.module.pin_count = PICO_DEFAULT_RS485_PIN_COUNT;
	memcpy(rs485.module.name, RS485_MODULE_NAME, sizeof(RS485_MODULE_NAME));

	ana_config_pio_init(&rs485);
	ana_config_pio_dma_init(&rs485);
}

struct ana_config_system *ana_rs485_get_config(void)
{
	return &rs485;
}
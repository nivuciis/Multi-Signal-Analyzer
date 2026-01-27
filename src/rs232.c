/*******************************************************************
 * @file sr232.c
 *
 * @brief RS232 communication tests
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 07/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/
#include "bring_up_rs232.pio.h"
#include "config_pio.h"
#include "rs232.h"
#include "log.h"

#include <stdio.h>
#include <string.h>

#define RS485_SAMPLES       256
#define RS485_GPIO_PIN_BASE 24
#define RS485_PIN_COUNT     2
#define RS232_MODULE_NAME   "RS232"

static struct ana_config_system rs232;
static uint16_t dma_rs232_buffer[RS485_SAMPLES];

static void __not_in_flash_func(rs232_capture_finished)(void)
{
	uint32_t ints = dma_hw->ints0;
	dma_hw->ints0 = ints;

	if (ints & (1u << rs232.dma.instance)) {
		dma_channel_acknowledge_irq0(rs232.dma.instance);

		/**
		 * @note Never restart the DMA here if using wait_for_finish_blocking, cus it'll
		 * cause an infinite loop.
		 */

		rs232.dma.has_complete = true;

		pio_sm_set_enabled(rs232.pio.instance, rs232.pio.sm, false);

		log_inf(RS232_MODULE_NAME, "RS232 DMA IRQ triggered");
	}

	/**
	 * @note If use the dma_chan1 set the config bellow as  above
	 * if (ints & (1u << rs232.dma.instance)) { ... }
	 *
	 */
}

void ana_rs232_init(void)
{
	log_inf(RS232_MODULE_NAME, "Initializing...");

	rs232.pio.pio_program = &bring_up_rs232_program;
	rs232.pio.get_default_cfg_func = bring_up_rs232_program_get_default_config;

	/**
	 * @note To use wait_for_finish_blocking you can't use a callback
	 *
	 * use NULL for blocking mode
	 */

	/**< rs232_config.dma_callback = rs232_capture_finished;  */

	rs232.dma.callback = NULL; 

	rs232.dma.dma_buffer = dma_rs232_buffer;
	rs232.module.samples = RS485_SAMPLES;
	rs232.module.pin_base = RS485_GPIO_PIN_BASE;
	rs232.module.pin_count = RS485_PIN_COUNT;
	memcpy(rs232.module.name, RS232_MODULE_NAME, sizeof(RS232_MODULE_NAME));

	ana_config_pio_init(&rs232);
	ana_config_pio_dma_init(&rs232);
}

struct ana_config_system *ana_rs232_get_config(void)
{
	return &rs232;
}
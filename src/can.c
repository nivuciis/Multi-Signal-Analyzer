/*******************************************************************
 * @file can.c
 *
 * @brief CAN bus control tests
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 08/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/
#include "bring_up_can.pio.h"
#include "config_pio.h"
#include "can.h"
#include "log.h"

#include <stdio.h>
#include <string.h>

#define CAN_SAMPLES       256
#define CAN_MODULE_NAME   "CAN"

static struct ana_config_system can;
static uint16_t dma_can_buffer[CAN_SAMPLES];

static void __not_in_flash_func(can_capture_finished)(void)
{
	uint32_t ints = dma_hw->ints0;
	dma_hw->ints0 = ints;

	if (ints & (1u << can.dma.instance)) {
		dma_channel_acknowledge_irq0(can.dma.instance);

		/**
		 * @note Never restart the DMA here if using wait_for_finish_blocking, cus it'll
		 * cause an infinite loop.
		 */

		can.dma.has_complete = true;

		pio_sm_set_enabled(can.pio.instance, can.pio.sm, false);

		log_inf(CAN_MODULE_NAME, "CAN DMA IRQ triggered");
	}

	/**
	 * @note If use the dma_chan1 set the config bellow as  above
	 * if (ints & (1u << can.dma.instance)) { ... }
	 *
	 */
}

void ana_can_init(void)
{
	log_inf(CAN_MODULE_NAME, "Initializing...");

	can.pio.instance = pio0;
	can.pio.pio_program = &bring_up_can_program;
	can.pio.get_default_cfg_func = bring_up_can_program_get_default_config;

	/**
	 * @note To use wait_for_finish_blocking you can't use a callback
	 *
	 * use NULL for blocking mode
	 */

	/**< can.dma.callback = can_capture_finished;  */

	can.dma.callback = NULL;

	can.dma.dma_buffer = dma_can_buffer;
	can.module.samples = CAN_SAMPLES;
	can.module.pin_base = PICO_DEFAULT_CAN_PIN_BASE;
	can.module.pin_count = PICO_DEFAULT_CAN_PIN_COUNT;
	memcpy(can.module.name, CAN_MODULE_NAME, sizeof(CAN_MODULE_NAME));

	ana_config_pio_init(&can);
	ana_config_pio_dma_init(&can);
}

struct ana_config_system *ana_can_get_config(void)
{
	return &can;
}
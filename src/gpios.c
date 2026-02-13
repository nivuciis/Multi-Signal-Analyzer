/*******************************************************************
 * @file gpios.c
 *
 * @brief GPIOs module test
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 09/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/

#include "bring_up_gpios.pio.h"
#include "config_pio.h"
#include "gpios.h"
#include "log.h"

#include <stdio.h>
#include <string.h>

#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/pio.h>
#include <pico/time.h>
#include <pico/types.h>

#define GPIO_SAMPLES    256
#define GPIOS_PIN_BASE  8
#define GPIOS_PIN_COUNT 12
#define GPIO_MODULE_NAME "GPIOS"

static struct ana_config_system gpio;
static uint16_t dma_gpios_buffer[GPIO_SAMPLES];

static void __not_in_flash_func(gpios_capture_finished)(void)
{
	uint32_t ints = dma_hw->ints0;
	dma_hw->ints0 = ints;

	if (ints & (1u << gpio.dma.instance)) {
		dma_channel_acknowledge_irq0(gpio.dma.instance);

		/**
		 * @note Never restart the DMA here if using wait_for_finish_blocking, cus it'll
		 * cause an infinite loop.
		 */

		gpio.dma.has_complete = true;

		pio_sm_set_enabled(gpio.pio.instance, gpio.pio.sm, false);

		log_inf(GPIO_MODULE_NAME, "GPIO DMA IRQ triggered");
	}

	/**
	 * @note If use the dma_chan2 set the config bellow as  above
	 * if (ints & (1u << gpio.dma.instance2)) { ... }
	 *
	 */
}

void ana_gpios_init(void)
{
	log_inf(GPIO_MODULE_NAME, "Initializing...");

	gpio.pio.instance = pio1;
	gpio.pio.pio_program = &bring_up_gpios_program;
	gpio.pio.get_default_cfg_func = bring_up_gpios_program_get_default_config;

	/**
	 * @note To use wait_for_finish_blocking you can't use a callback
	 *
	 * use NULL for blocking mode
	 *
	 * < gpio_config.dma_callback = gpios_capture_finished;  
	 */

	gpio.dma.callback = NULL; 

	gpio.dma.dma_buffer = dma_gpios_buffer;
	gpio.module.samples = GPIO_SAMPLES;
	gpio.module.pin_base = GPIOS_PIN_BASE;
	gpio.module.pin_count = GPIOS_PIN_COUNT;
	gpio.module.sample_rate_hz = 125000; /* 125 kHz */
	memcpy(gpio.module.name, GPIO_MODULE_NAME, sizeof(GPIO_MODULE_NAME));

	ana_config_pio_init(&gpio);
	ana_config_pio_dma_init(&gpio);
}

struct ana_config_system *ana_gpios_get_config(void)
{
	return &gpio;
}
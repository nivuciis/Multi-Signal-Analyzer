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

#include <stdio.h>
#include <string.h>

#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/pio.h>
#include <pico/time.h>
#include <pico/types.h>

#define GPIO_SAMPLES 1024

static struct ana_config_pio gpio_config;
static uint16_t dma_gpios_buffer[GPIO_SAMPLES];

uint16_t *ana_gpios_get_buffer()
{
	return dma_gpios_buffer;
}

static void __not_in_flash_func(gpios_capture_finished)(void)
{
	uint32_t ints = dma_hw->ints0;
	dma_hw->ints0 = ints;

	if (ints & (1u << gpio_config.dma_chan0)) {
		dma_channel_acknowledge_irq0(gpio_config.dma_chan0);

		/**
		 * @note Never restart the DMA here if using wait_for_finish_blocking, cus it'll
		 * cause an infinite loop.
		 */

		gpio_config.dma_complete = true;

		pio_sm_set_enabled(gpio_config.pio, gpio_config.sm, false);

		printf("GPIO DMA IRQ triggered\n");
	}

	/**
	 * @note If use the dma_chan1 set the config bellow as  above
	 * if (ints & (1u << gpio_config.dma_chan1)) { ... }
	 *
	 */
}

void ana_gpios_init(void)
{
	printf("GPIOS init\n");

	gpio_config.pio_program = &bring_up_gpios_program;
	gpio_config.get_default_cfg_func = bring_up_gpios_program_get_default_config;

	/**
	 * @note To use wait_for_finish_blocking you can't use a callback
	 *
	 * use NULL for blocking mode
	 */
	/**< gpio_config.dma_callback = gpios_capture_finished;  */

	gpio_config.dma_callback = NULL; // Use NULL para modo bloqueante

	gpio_config.dma_buffer = dma_gpios_buffer;
	gpio_config.pin_base = 9;
	gpio_config.pin_count = 2;
	gpio_config.samples = 10;

	ana_config_pio_init(&gpio_config);
	ana_config_pio_dma_init(&gpio_config);
}

void ana_gpios_get_data(void)
{
	printf("Start GPIO capture\n");

	pio_sm_set_enabled(gpio_config.pio, gpio_config.sm, false);

	memset(gpio_config.dma_buffer, 0, sizeof(uint16_t) * gpio_config.samples);

	pio_sm_clear_fifos(gpio_config.pio, gpio_config.sm);
	pio_sm_restart(gpio_config.pio, gpio_config.sm);

	dma_channel_configure(gpio_config.dma_chan0, &gpio_config.dma_chan0_cfg,
			      gpio_config.dma_buffer, &gpio_config.pio->rxf[gpio_config.sm],
			      gpio_config.samples, false);
	dma_channel_start(gpio_config.dma_chan0);

	pio_sm_set_enabled(gpio_config.pio, gpio_config.sm, true);
	dma_channel_wait_for_finish_blocking(gpio_config.dma_chan0);
	pio_sm_set_enabled(gpio_config.pio, gpio_config.sm, false);

	printf("DMA transfer finished\n");

	if (dma_channel_is_busy(gpio_config.dma_chan0)) {
		printf("ERROR: DMA still busy! Aborting...\n");
		dma_channel_abort(gpio_config.dma_chan0);
	} else {
		printf("DMA completed successfully\n");
	}
}

void ana_gpios_get_data_v2(void)
{
	printf("Start GPIO capture (v2)\n");

	pio_sm_set_enabled(gpio_config.pio, gpio_config.sm, false);
	memset(gpio_config.dma_buffer, 0, sizeof(uint16_t) * gpio_config.samples);
	pio_sm_clear_fifos(gpio_config.pio, gpio_config.sm);
	pio_sm_restart(gpio_config.pio, gpio_config.sm);

	ana_config_pio_dma_start(&gpio_config);

	pio_sm_set_enabled(gpio_config.pio, gpio_config.sm, true);
	ana_config_pio_dma_wait(&gpio_config);
	pio_sm_set_enabled(gpio_config.pio, gpio_config.sm, false);

	printf("DMA transfer finished (v2)\n");
}

void ana_gpios_print_data(void)
{
	if (gpio_config.pin_count == 0 || gpio_config.pin_count > 16) {
		printf("ERROR: Invalid pin_count: %u\n", gpio_config.pin_count);
		return;
	}

	printf("GPIO Data (each column is a pin, left-to-right = pin_map[0]..pin_map[n-1]):\n");

	printf("Pins:   ");
	for (uint i = 0; i < gpio_config.pin_count; i++) {
		printf("%3u", gpio_config.pin_base + i);
	}
	printf("\n");

	printf("Sample: ");
	for (uint i = 0; i < gpio_config.pin_count; i++) {
		printf("---");
	}
	printf("\n");

	uint samples_to_show = gpio_config.samples;
	if (samples_to_show > 20) {
		samples_to_show = 20;
		printf("(Showing first 20 of %u samples)\n", gpio_config.samples);
	}

	for (uint s = 0; s < samples_to_show; s++) {
		uint16_t v = gpio_config.dma_buffer[s];
		printf("%6u:", s);

		/* Print bits for each pin in pin_map order (pin_map[0] -> first bit printed) */
		for (uint i = 0; i < gpio_config.pin_count; i++) {
			uint bit = (v >> i) & 0x1u;
			printf("  %u", bit);
		}

		printf("  0x%04X\n", v);
	}

	if (gpio_config.samples > 20) {
		printf("...\n");
		uint16_t v = gpio_config.dma_buffer[gpio_config.samples - 1];
		printf("%6u:", gpio_config.samples - 1);
		for (uint i = 0; i < gpio_config.pin_count; i++) {
			uint bit = (v >> i) & 0x1u;
			printf("  %u", bit);
		}
		printf("  0x%04X\n", v);
	}
}

void ana_gpios_test_pio_direct(void)
{
	printf("\n=== Testing PIO without DMA ===\n");

	pio_sm_set_enabled(gpio_config.pio, gpio_config.sm, false);
	pio_sm_clear_fifos(gpio_config.pio, gpio_config.sm);
	pio_sm_restart(gpio_config.pio, gpio_config.sm);
	pio_sm_set_enabled(gpio_config.pio, gpio_config.sm, true);

	sleep_ms(100);

	uint fifo_level = pio_sm_get_rx_fifo_level(gpio_config.pio, gpio_config.sm);
	printf("PIO FIFO level: %u\n", fifo_level);

	if (fifo_level > 0) {
		uint16_t data = pio_sm_get(gpio_config.pio, gpio_config.sm);
		printf("Direct PIO read: 0x%04X\n", data);

		for (uint i = 0; i < gpio_config.pin_count; i++) {
			uint bit = (data >> i) & 0x1u;
			printf("  Pin %u: %u\n", gpio_config.pin_base + i, bit);
		}
	} else {
		printf("ERROR: No data in PIO FIFO!\n");
		printf("Check: \n");
		printf("  1. PIO program loaded correctly\n");
		printf("  2. Pins configured as inputs\n");
		printf("  3. Clock divider set correctly\n");
	}

	pio_sm_set_enabled(gpio_config.pio, gpio_config.sm, false);
}

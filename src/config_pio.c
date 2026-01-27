/*******************************************************************
 * @file config_pio.c
 *
 * @brief Configuration for PIO programs
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 08/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/
#include "config_pio.h"
#include "log.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <hardware/address_mapped.h>
#include <hardware/clocks.h>
#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/pio.h>
#include <pico/error.h>


void ana_config_pio_init(struct ana_config_system *config)
{
	log_debug(config->module.name, "Initializing PIO...");

	int pin;

	config->pio.instance = pio0;
	config->pio.sm = pio_claim_unused_sm(config->pio.instance, true);

	config->dma.has_complete = false;

	pio_sm_set_enabled(config->pio.instance, config->pio.sm, false);
	config->pio.pio_offset = pio_add_program(config->pio.instance, config->pio.pio_program);
	pio_sm_config sm_cfg = config->pio.get_default_cfg_func(config->pio.pio_offset);

	for (int i = 0; i < config->module.pin_count; i++) {
		pin = config->module.pin_base + i;
		pio_gpio_init(config->pio.instance, pin);
		gpio_pull_down(pin);
		// //_REG_(PADS_BANK0_BASE + (pin*4) &= 1u<<3);
		// io_rw_32 *pads_reg = (io_rw_32 *)(PADS_BANK0_BASE + (pin * 4));
		// *pads_reg &= ~(1u << 3);
		// asm volatile("nop");
		// *pads_reg &= (1u << 2);
		// asm volatile("nop");
		// asm volatile("nop");
	}

	pio_sm_set_consecutive_pindirs(config->pio.instance, config->pio.sm,
				       config->module.pin_base, config->module.pin_count, false);

	sm_config_set_wrap(&sm_cfg, config->pio.pio_offset,
			   config->pio.pio_offset + config->pio.pio_program->length - 1);
	sm_config_set_in_pins(&sm_cfg, config->module.pin_base);
	sm_config_set_in_shift(&sm_cfg, false, true, config->module.pin_count);

	float clkdiv = clock_get_hz(clk_sys) / 10000.0f;
	sm_config_set_clkdiv(&sm_cfg, clkdiv);

	sm_config_set_fifo_join(&sm_cfg, PIO_FIFO_JOIN_RX);

	pio_sm_init(config->pio.instance, config->pio.sm, config->pio.pio_offset, &sm_cfg);
	pio_sm_clear_fifos(config->pio.instance, config->pio.sm);
	pio_sm_set_enabled(config->pio.instance, config->pio.sm, false);
}

void ana_config_pio_dma_init(struct ana_config_system *config)
{
	log_debug(config->module.name, "Initializing DMA...");

	config->dma.instance = dma_claim_unused_channel(true);

	config->dma.instance_cfg = dma_channel_get_default_config(config->dma.instance);
	channel_config_set_read_increment(&config->dma.instance_cfg, false);
	channel_config_set_write_increment(&config->dma.instance_cfg, true);
	channel_config_set_transfer_data_size(&config->dma.instance_cfg, DMA_SIZE_16);
	channel_config_set_dreq(&config->dma.instance_cfg,
				pio_get_dreq(config->pio.instance, config->pio.sm, false));

	dma_channel_configure(config->dma.instance, &config->dma.instance_cfg,
			      config->dma.dma_buffer, &config->pio.instance->rxf[config->pio.sm],
			      config->module.samples, false);
}

void ana_config_pio_dma_start(struct ana_config_system *config)
{
	if (config->dma.callback) {
		irq_set_exclusive_handler(DMA_IRQ_0, config->dma.callback);
		irq_set_enabled(DMA_IRQ_0, true);
		dma_channel_set_irq0_enabled(config->dma.instance, true);
	}

	config->dma.has_complete = false;

	dma_channel_configure(config->dma.instance, &config->dma.instance_cfg,
			      config->dma.dma_buffer, &config->pio.instance->rxf[config->pio.sm],
			      config->module.samples, false);

	dma_channel_start(config->dma.instance);
}

void ana_config_pio_dma_wait(struct ana_config_system *config)
{
	dma_channel_wait_for_finish_blocking(config->dma.instance);

	config->dma.has_complete = true;
}

bool ana_config_pio_dma_is_busy(struct ana_config_system *config)
{
	return dma_channel_is_busy(config->dma.instance);
}

void ana_config_pio_dma_abort(struct ana_config_system *config)
{
	dma_channel_abort(config->dma.instance);

	if (config->dma.callback) {
		dma_channel_set_irq0_enabled(config->dma.instance, false);
		irq_set_enabled(DMA_IRQ_0, false);
		irq_remove_handler(DMA_IRQ_0, config->dma.callback);
	}

	config->dma.has_complete = true;
}

void ana_config_pio_get_data(struct ana_config_system *config)
{
	log_debug(config->module.name, "Start GPIO capture");

	pio_sm_set_enabled(config->pio.instance, config->pio.sm, false);

	memset(config->dma.dma_buffer, 0, sizeof(uint16_t) * config->module.samples);

	pio_sm_clear_fifos(config->pio.instance, config->pio.sm);
	pio_sm_restart(config->pio.instance, config->pio.sm);

	dma_channel_configure(config->dma.instance, &config->dma.instance_cfg,
			      config->dma.dma_buffer, &config->pio.instance->rxf[config->pio.sm],
			      config->module.samples, false);
	dma_channel_start(config->dma.instance);

	pio_sm_set_enabled(config->pio.instance, config->pio.sm, true);
	dma_channel_wait_for_finish_blocking(config->dma.instance);
	pio_sm_set_enabled(config->pio.instance, config->pio.sm, false);
	log_debug(config->module.name, "DMA transfer finished");

	if (dma_channel_is_busy(config->dma.instance)) {
		log_err(config->module.name, "DMA still busy! Aborting...");
		dma_channel_abort(config->dma.instance);
	} else {
		log_debug(config->module.name, "DMA completed successfully");
	}
}

void ana_config_pio_get_data_v2(struct ana_config_system *config)
{
	log_debug(config->module.name, "Start GPIO capture (v2)");

	pio_sm_set_enabled(config->pio.instance, config->pio.sm, false);
	memset(config->dma.dma_buffer, 0, sizeof(uint16_t) * config->module.samples);
	pio_sm_clear_fifos(config->pio.instance, config->pio.sm);
	pio_sm_restart(config->pio.instance, config->pio.sm);

	ana_config_pio_dma_start(config);

	pio_sm_set_enabled(config->pio.instance, config->pio.sm, true);
	ana_config_pio_dma_wait(config);
	pio_sm_set_enabled(config->pio.instance, config->pio.sm, false);
	log_debug(config->module.name, "DMA transfer finished (v2)");
}

void ana_config_pio_print_data(struct ana_config_system *config)
{
	if (config->module.pin_count == 0 || config->module.pin_count > 16) {
		log_err(config->module.name, "Invalid pin_count: %u", config->module.pin_count);
		return;
	}

	log_debug(config->module.name, "GPIO Data (each column is a pin, left-to-right = pin_map[0]..pin_map[n-1]):");
	printf("Pins:   ");
	for (uint i = 0; i < config->module.pin_count; i++) {
		printf("%3u", config->module.pin_base + i);
	}
	printf("\n");

	printf("Sample: ");
	for (uint i = 0; i < config->module.pin_count; i++) {
		printf("---");
	}
	printf("\n");

	uint samples_to_show = config->module.samples;
	if (samples_to_show > 20) {
		samples_to_show = 20;
		printf("(Showing first 20 of %u samples)\n", config->module.samples);
	}

	for (uint s = 0; s < samples_to_show; s++) {
		uint16_t v = config->dma.dma_buffer[s];
		printf("%6u:", s);

		/* Print bits for each pin in pin_map order (pin_map[0] -> first bit printed) */
		for (uint i = 0; i < config->module.pin_count; i++) {
			uint bit = (v >> i) & 0x1u;
			printf("  %u", bit);
		}

		printf("  0x%04X\n", v);
	}

	if (config->module.samples > 20) {
		printf("...\n");
		uint16_t v = config->dma.dma_buffer[config->module.samples - 1];
		printf("%6u:", config->module.samples - 1);
		for (uint i = 0; i < config->module.pin_count; i++) {
			uint bit = (v >> i) & 0x1u;
			printf("  %u", bit);
		}
		printf("  0x%04X\n", v);
	}
}

void ana_config_pio_test_pio_direct(struct ana_config_system *config)
{
	log_debug(config->module.name, "Testing PIO without DMA");

	pio_sm_set_enabled(config->pio.instance, config->pio.sm, false);
	pio_sm_clear_fifos(config->pio.instance, config->pio.sm);
	pio_sm_restart(config->pio.instance, config->pio.sm);
	pio_sm_set_enabled(config->pio.instance, config->pio.sm, true);

	sleep_ms(100);

	uint fifo_level = pio_sm_get_rx_fifo_level(config->pio.instance, config->pio.sm);
	log_debug(config->module.name, "PIO FIFO level: %u", fifo_level);

	if (fifo_level > 0) {
		uint16_t data = pio_sm_get(config->pio.instance, config->pio.sm);
		log_debug(config->module.name, "Direct PIO read: 0x%04X", data);
		for (uint i = 0; i < config->module.pin_count; i++) {
			uint bit = (data >> i) & 0x1u;
			printf("  Pin %u: %u\n", config->module.pin_base + i, bit);
		}
	} else {
		log_err(config->module.name, "No data in PIO FIFO!");
		log_err(config->module.name, "Check: ");
		log_err(config->module.name, "  1. PIO program loaded correctly");
		log_err(config->module.name, "  2. Pins configured as inputs");
		log_err(config->module.name, "  3. Clock divider set correctly");
	}

	pio_sm_set_enabled(config->pio.instance, config->pio.sm, false);
}

uint16_t *ana_config_pio_get_buffer(struct ana_config_system *config)
{
	return config->dma.dma_buffer;
}

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

#include <stdbool.h>
#include <stdio.h>

#include <hardware/clocks.h>
#include <hardware/dma.h>
#include <hardware/irq.h>
#include <hardware/pio.h>
#include <pico/error.h>

void ana_config_pio_init(struct ana_config_pio *config)
{
	config->pio = pio0;
	config->sm = pio_claim_unused_sm(config->pio, true);

	config->dma_complete = false;

	pio_sm_set_enabled(config->pio, config->sm, false);

	config->pio_offset = pio_add_program(config->pio, config->pio_program);
	pio_sm_config sm_cfg = config->get_default_cfg_func(config->pio_offset);

	for (int i = 0; i < config->pin_count; i++) {
		pio_gpio_init(config->pio, config->pin_base + i);
	}

	pio_sm_set_consecutive_pindirs(config->pio, config->sm, config->pin_base, config->pin_count,
				       false);

	sm_config_set_wrap(&sm_cfg, config->pio_offset,
			   config->pio_offset + config->pio_program->length - 1);
	sm_config_set_in_pins(&sm_cfg, config->pin_base);
	sm_config_set_in_shift(&sm_cfg, false, true, config->pin_count);

	float clkdiv = clock_get_hz(clk_sys) / 1000000.0f;
	sm_config_set_clkdiv(&sm_cfg, clkdiv);

	sm_config_set_fifo_join(&sm_cfg, PIO_FIFO_JOIN_RX);

	pio_sm_init(config->pio, config->sm, config->pio_offset, &sm_cfg);
	pio_sm_clear_fifos(config->pio, config->sm);
	pio_sm_set_enabled(config->pio, config->sm, false);
}

void ana_config_pio_dma_init(struct ana_config_pio *config)
{
	config->dma_chan0 = dma_claim_unused_channel(true);

	config->dma_chan0_cfg = dma_channel_get_default_config(config->dma_chan0);
	channel_config_set_read_increment(&config->dma_chan0_cfg, false);
	channel_config_set_write_increment(&config->dma_chan0_cfg, true);
	channel_config_set_transfer_data_size(&config->dma_chan0_cfg, DMA_SIZE_16);
	channel_config_set_dreq(&config->dma_chan0_cfg,
				pio_get_dreq(config->pio, config->sm, false));

	dma_channel_configure(config->dma_chan0, &config->dma_chan0_cfg, config->dma_buffer,
			      &config->pio->rxf[config->sm], config->samples, false);
}

void ana_config_pio_dma_start(struct ana_config_pio *config)
{
	if (config->dma_callback) {
		irq_set_exclusive_handler(DMA_IRQ_0, config->dma_callback);
		irq_set_enabled(DMA_IRQ_0, true);
		dma_channel_set_irq0_enabled(config->dma_chan0, true);
	}

	config->dma_complete = false;

	dma_channel_configure(config->dma_chan0, &config->dma_chan0_cfg, config->dma_buffer,
			      &config->pio->rxf[config->sm], config->samples, false);

	dma_channel_start(config->dma_chan0);
}

void ana_config_pio_dma_wait(struct ana_config_pio *config)
{
	dma_channel_wait_for_finish_blocking(config->dma_chan0);
	config->dma_complete = true;
}

bool ana_config_pio_dma_is_busy(struct ana_config_pio *config)
{
	return dma_channel_is_busy(config->dma_chan0);
}

void ana_config_pio_dma_abort(struct ana_config_pio *config)
{
	dma_channel_abort(config->dma_chan0);

	if (config->dma_callback) {
		dma_channel_set_irq0_enabled(config->dma_chan0, false);
		irq_set_enabled(DMA_IRQ_0, false);
		irq_remove_handler(DMA_IRQ_0, config->dma_callback);
	}

	config->dma_complete = true;
}

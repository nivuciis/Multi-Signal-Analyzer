/*******************************************************************
 * @file module.c
 * @brief Module implementation
 *
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 04/02/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/
#include "log.h"
#include "module.h"
#include "sigrok_handler.h"

#include <stdint.h>

#include <hardware/pio.h>

void ana_module_pio_init(struct ana_module_system *config)
{
	log_debug(config->module.name, "Initializing PIO...");

	struct pulseview_sample_config *cfg = ana_sigrok_get_sample_config();
	int pin;

	config->pio.sm = pio_claim_unused_sm(config->pio.instance, true);
	config->dma.has_complete = false;

	pio_sm_set_enabled(config->pio.instance, config->pio.sm, false);
	config->pio.pio_offset = pio_add_program(config->pio.instance, config->pio.pio_program);
	pio_sm_config sm_cfg = config->pio.get_default_cfg_func(config->pio.pio_offset);

	for (int i = 0; i < config->module.pin_count; i++) {
		pin = config->module.pin_base + i;
		pio_gpio_init(config->pio.instance, pin);
		gpio_pull_up(pin);
	}

	pio_sm_set_consecutive_pindirs(config->pio.instance, config->pio.sm,
				       config->module.pin_base, config->module.pin_count, false);
	sm_config_set_wrap(&sm_cfg, config->pio.pio_offset,
			   config->pio.pio_offset + config->pio.pio_program->length - 1);
	sm_config_set_in_pins(&sm_cfg, config->module.pin_base);
	sm_config_set_in_shift(&sm_cfg, false, true, config->module.pin_count);

	float clkdiv = clock_get_hz(clk_sys) / (float)cfg->sample_rate_hz;
	sm_config_set_clkdiv(&sm_cfg, clkdiv);

	sm_config_set_fifo_join(&sm_cfg, PIO_FIFO_JOIN_RX);

	if (config->pio.jmp_pin > 0) {
		sm_config_set_jmp_pin(&sm_cfg, config->pio.jmp_pin);
	}

	pio_sm_init(config->pio.instance, config->pio.sm, config->pio.pio_offset, &sm_cfg);
	pio_sm_clear_fifos(config->pio.instance, config->pio.sm);
	pio_sm_set_enabled(config->pio.instance, config->pio.sm, false);
}

void ana_module_dma_init(struct ana_module_system *config)
{
	log_debug(config->module.name, "Initializing DMA...");

	config->dma.instance = dma_claim_unused_channel(true);

	config->dma.instance_cfg = dma_channel_get_default_config(config->dma.instance);
	channel_config_set_read_increment(&config->dma.instance_cfg, false);
	channel_config_set_write_increment(&config->dma.instance_cfg, true);
	channel_config_set_transfer_data_size(&config->dma.instance_cfg, DMA_SIZE_16);
	channel_config_set_dreq(&config->dma.instance_cfg,
				pio_get_dreq(config->pio.instance, config->pio.sm, false));
}

void ana_module_pio_dma_start(struct ana_module_system *config)
{
	struct pulseview_sample_config *cfg = ana_sigrok_get_sample_config();

	if (config->dma.callback) {
		irq_set_exclusive_handler(DMA_IRQ_0, config->dma.callback);
		irq_set_enabled(DMA_IRQ_0, true);
		dma_channel_set_irq0_enabled(config->dma.instance, true);
	}

	config->dma.has_complete = false;

	dma_channel_configure(config->dma.instance, &config->dma.instance_cfg,
			      config->dma.dma_buffer, &config->pio.instance->rxf[config->pio.sm],
			      cfg->samples, false);

	dma_channel_start(config->dma.instance);
	pio_sm_set_enabled(config->pio.instance, config->pio.sm, true);
}

void ana_module_pio_dma_wait(struct ana_module_system *config)
{
	dma_channel_wait_for_finish_blocking(config->dma.instance);

	config->dma.has_complete = true;
}

bool ana_module_pio_dma_is_busy(struct ana_module_system *config)
{
	return dma_channel_is_busy(config->dma.instance);
}

void ana_module_pio_dma_abort(struct ana_module_system *config)
{
	dma_channel_abort(config->dma.instance);

	if (config->dma.callback) {
		dma_channel_set_irq0_enabled(config->dma.instance, false);
		irq_set_enabled(DMA_IRQ_0, false);
		irq_remove_handler(DMA_IRQ_0, config->dma.callback);
	}

	config->dma.has_complete = true;
}

void ana_module_set_sample_rate(struct ana_module_system *config)
{
	struct pulseview_sample_config *cfg = ana_sigrok_get_sample_config();
	float clkdiv = clock_get_hz(clk_sys) / (float)cfg->sample_rate_hz;
	pio_sm_set_clkdiv(config->pio.instance, config->pio.sm, clkdiv);
}

void ana_module_pio_reload(struct ana_module_system *config, const pio_program_t *new_program,
			   pio_sm_config (*new_cfg_func)(uint8_t offset), uint8_t jmp_pin)
{
	pio_sm_set_enabled(config->pio.instance, config->pio.sm, false);
	pio_remove_program(config->pio.instance, config->pio.pio_program, config->pio.pio_offset);

	config->pio.pio_program = new_program;
	config->pio.get_default_cfg_func = new_cfg_func;
	config->pio.jmp_pin = jmp_pin;

	ana_module_pio_init(config);
}

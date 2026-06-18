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
#include "handles/sigrok_handler.h"
#include "log.h"
#include "module.h"

#include <stdint.h>

#include <hardware/dma.h>
#include <hardware/pio.h>

/*
 * Chained ping-pong capture
 * --------------------------
 * Two DMA channels (A = instance, B = instance_b) each drain the PIO RX FIFO
 * into their own buffer and chain to the other on completion, so the PIO never
 * stalls and the sample stream is gap-free. A DMA_IRQ_0 handler recycles the
 * finished channel (resets write addr + transfer count, non-triggering, so it
 * is ready when its partner chains back) and bumps pp_produced. Core 1 reads
 * pp_produced/pp_consumed to know which buffer is ready, and pp_overflow flags
 * the producer lapping the consumer (USB too slow → soft overflow).
 */
#define ANA_PP_MAX_MODULES 3
static struct ana_module_system *pp_modules[ANA_PP_MAX_MODULES];
static int pp_module_count;
static bool pp_irq_installed;

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
		gpio_pull_down(pin);
	}

	pio_sm_set_consecutive_pindirs(config->pio.instance, config->pio.sm,
				       config->module.pin_base, config->module.pin_count, false);
	sm_config_set_in_pins(&sm_cfg, config->module.pin_base);
	/* Autopush after exactly pin_count bits so the PIO needs a single
	 * `in pins,N` per sample (1 PIO clock). A padding `in null` to reach a
	 * fixed 16-bit threshold would cost a second clock and halve the real
	 * sample rate (and cap it at 60 MS/s instead of 120, for example). */
	sm_config_set_in_shift(&sm_cfg, false, true, config->module.pin_count);

	float clkdiv = clock_get_hz(clk_sys) / (float)cfg->sample_rate_hz;
	sm_config_set_clkdiv(&sm_cfg, clkdiv);

	sm_config_set_fifo_join(&sm_cfg, PIO_FIFO_JOIN_RX);

	if (config->pio.jmp_pin != 0xFF) {
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

	config->dma.has_complete = false;

	pio_sm_set_enabled(config->pio.instance, config->pio.sm, false);
	pio_sm_clear_fifos(config->pio.instance, config->pio.sm);
	pio_sm_restart(config->pio.instance, config->pio.sm);
	pio_sm_exec(config->pio.instance, config->pio.sm, pio_encode_jmp(config->pio.pio_offset));

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
	pio_sm_set_enabled(config->pio.instance, config->pio.sm, false);
	dma_channel_abort(config->dma.instance);

	config->dma.has_complete = true;
}

void ana_module_set_sample_rate(struct ana_module_system *config)
{
	struct pulseview_sample_config *cfg = ana_sigrok_get_sample_config();
	float clkdiv = clock_get_hz(clk_sys) / (float)cfg->sample_rate_hz;
	pio_sm_set_clkdiv(config->pio.instance, config->pio.sm, clkdiv);
}

inline static void ana_module_verify_pp_irq(struct ana_module_dma *dma, uint8_t instance)
{
	if (!dma_channel_get_irq0_status(instance)) {
		dma_channel_acknowledge_irq0(instance);
		dma->pp_produced++;
		if (dma->pp_produced - dma->pp_consumed >= 2u) {
			dma->pp_overflow = true;
		}
	}
}

/* The channels recycle themselves in hardware: the write-address ring wraps
 * back to the buffer base and TRANS_COUNT reloads on every chain trigger, so
 * this handler only has to account for produced buffers. Reprogramming the
 * channel here would race the partner's chain re-trigger at high rates. */
static void ana_module_pp_irq(void)
{
	for (int i = 0; i < pp_module_count; i++) {
		struct ana_module_system *m = pp_modules[i];

		ana_module_verify_pp_irq(&m->dma, m->dma.instance);
		ana_module_verify_pp_irq(&m->dma, m->dma.instance_b);
	}
}

void ana_module_pingpong_init(struct ana_module_system *config, uint16_t *buf_a, uint16_t *buf_b,
			      uint32_t chunk)
{
	config->dma.buf_a = buf_a;
	config->dma.buf_b = buf_b;
	config->dma.pp_chunk = chunk;

	if (pp_module_count >= ANA_PP_MAX_MODULES) {
		log_err(config->module.name, "Too many ping-pong modules");
		return;
	}

	config->dma.instance_b = (uint8_t)dma_claim_unused_channel(true);

	pp_modules[pp_module_count] = config;
	pp_module_count++;
}

static void ana_module_pp_configure(struct ana_module_system *config, uint8_t chan,
				    uint8_t chain_to, uint16_t *buf)
{
	dma_channel_config c = dma_channel_get_default_config(chan);
	channel_config_set_read_increment(&c, false);
	channel_config_set_write_increment(&c, true);
	channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
	channel_config_set_dreq(&c, pio_get_dreq(config->pio.instance, config->pio.sm, false));
	channel_config_set_chain_to(&c, chain_to);
	/* Hardware write-address ring: the address can never leave the buffer,
	 * even if the partner's chain re-triggers this channel before the IRQ
	 * handler runs. Requires pp_chunk * sizeof(uint16_t) to be a power of
	 * two and the buffers aligned to that size. */
	channel_config_set_ring(&c, true,
				(uint)__builtin_ctz(config->dma.pp_chunk * sizeof(uint16_t)));

	dma_channel_configure(chan, &c, buf, &config->pio.instance->rxf[config->pio.sm],
			      config->dma.pp_chunk, false);
	dma_channel_acknowledge_irq0(chan); /* drop stale status from a prior run */
	dma_channel_set_irq0_enabled(chan, true);
}

void ana_module_pingpong_start(struct ana_module_system *config)
{
	config->dma.pp_produced = 0;
	config->dma.pp_consumed = 0;
	config->dma.pp_overflow = false;

	ana_module_pp_configure(config, config->dma.instance, config->dma.instance_b,
				config->dma.buf_a);
	ana_module_pp_configure(config, config->dma.instance_b, config->dma.instance,
				config->dma.buf_b);

	if (!pp_irq_installed) {
		irq_set_exclusive_handler(DMA_IRQ_0, ana_module_pp_irq);
		irq_set_enabled(DMA_IRQ_0, true);
		pp_irq_installed = true;
	}

	pio_sm_set_enabled(config->pio.instance, config->pio.sm, false);
	pio_sm_clear_fifos(config->pio.instance, config->pio.sm);
	pio_sm_restart(config->pio.instance, config->pio.sm);
	pio_sm_exec(config->pio.instance, config->pio.sm, pio_encode_jmp(config->pio.pio_offset));

	/* Start channel A only; B is launched by the chain when A completes. */
	dma_channel_start(config->dma.instance);
	pio_sm_set_enabled(config->pio.instance, config->pio.sm, true);
}

void ana_module_pingpong_stop(struct ana_module_system *config)
{
	pio_sm_set_enabled(config->pio.instance, config->pio.sm, false);
	dma_channel_set_irq0_enabled(config->dma.instance, false);
	dma_channel_set_irq0_enabled(config->dma.instance_b, false);
	dma_channel_abort(config->dma.instance);
	dma_channel_abort(config->dma.instance_b);
}

void ana_module_pio_reload(struct ana_module_system *config, const pio_program_t *new_program,
			   pio_sm_config (*new_cfg_func)(uint8_t offset), uint8_t jmp_pin)
{
	pio_sm_set_enabled(config->pio.instance, config->pio.sm, false);
	pio_remove_program_and_unclaim_sm(config->pio.pio_program, config->pio.instance,
					  config->pio.sm, config->pio.pio_offset);

	config->pio.pio_program = new_program;
	config->pio.get_default_cfg_func = new_cfg_func;
	config->pio.jmp_pin = jmp_pin;

	ana_module_pio_init(config);

	channel_config_set_dreq(&config->dma.instance_cfg,
				pio_get_dreq(config->pio.instance, config->pio.sm, false));
}

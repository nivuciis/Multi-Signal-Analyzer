/*******************************************************************
 * @file pwm.c
 *
 * @brief PWM communication tests
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 30/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/
#include "bring_up_pwm.pio.h"
#include "pwm.pio.h"
#include "config_pio.h"
#include "log.h"
#include "pwm.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <boards/pico2.h>
#include <hardware/clocks.h>
#include <pico/multicore.h>

#define PWM_SAMPLES       1024
#define PWM_GPIO_PIN_BASE 8
#define PWM_PIN_COUNT     1
#define PWM_MODULE_NAME   "pwm"

static struct ana_config_system pwm;
static struct _pwm_defs pwm_defs;
static uint16_t dma_pwm_buffer[PWM_SAMPLES];

static void __not_in_flash_func(pwm_capture_finished)(void)
{
	uint32_t ints = dma_hw->ints0;
	dma_hw->ints0 = ints;

	if (ints & (1u << pwm.dma.instance)) {
		dma_channel_acknowledge_irq0(pwm.dma.instance);

		/**
		 * @note Never restart the DMA here if using wait_for_finish_blocking, cus it'll
		 * cause an infinite loop.
		 */

		pwm.dma.has_complete = true;

		pio_sm_set_enabled(pwm.pio.instance, pwm.pio.sm, false);

		log_inf(PWM_MODULE_NAME, "PWM DMA IRQ triggered");
	}

	/**
	 * @note If use the dma_chan1 set the config bellow as  above
	 * if (ints & (1u << pwm.dma.instance)) { ... }
	 *
	 */
}

void ana_pwm_init(void)
{
	log_inf(PWM_MODULE_NAME, "Initializing...");

	 uint32_t sample_rate_hz = 1000000; // 1 MHz


	pwm.pio.instance = pio1;
	pwm.pio.pio_program = &bring_up_pwm_program;
	pwm.pio.get_default_cfg_func = bring_up_pwm_program_get_default_config;

	/**
	 * @note To use wait_for_finish_blocking you can't use a callback
	 *
	 * use NULL for blocking mode
	 */

	/**< rs232_config.dma_callback = rs232_capture_finished;  */

	pwm.dma.callback = NULL;

	pwm.dma.dma_buffer = dma_pwm_buffer;
	pwm.module.samples = PWM_SAMPLES;
	pwm.module.pin_base = PWM_GPIO_PIN_BASE;
	pwm.module.pin_count = PWM_PIN_COUNT;
	pwm.module.sample_rate_hz = sample_rate_hz;
	memcpy(pwm.module.name, PWM_MODULE_NAME, sizeof(PWM_MODULE_NAME));

	ana_config_pio_init(&pwm);
	ana_config_pio_dma_init(&pwm);

	/**
	 * @note: All code bellow is to set the pwm generation w\ PIO
	 *
	 */

	pwm_defs.pio = pio2;
	pwm_defs.sm = pio_claim_unused_sm(pwm_defs.pio, true);
	pwm_defs.offset = pio_add_program(pwm_defs.pio, &pwm_program);
	pwm_program_init(pwm_defs.pio,pwm_defs.sm,pwm_defs.offset,7);
	ana_pwm_sm_set_period(&pwm_defs, sample_rate_hz); // 1 kHz
	ana_pwm_sm_set_level(&pwm_defs, 500000);   // 50%

}

struct ana_config_system *ana_pwm_get_config(void)
{
	return &pwm;
}

void ana_pwm_measure_input_capture(void)
{
	struct ana_config_system *config = ana_pwm_get_config();
	uint16_t *buffer = config->dma.dma_buffer;
	uint32_t samples = config->module.samples;
	float sample_rate_hz = config->module.sample_rate_hz;

	uint32_t rise_edges[1000];
	uint32_t fall_edges[1000];
	uint32_t rise_count = 0;
	uint32_t fall_count = 0;
	uint32_t rise_start = 0;
	uint32_t rise_end = 0;
	uint32_t high_samples = 0;
	uint32_t period_samples = 0;
	uint32_t valid_periods = 0;
	uint32_t valid_cycles = 0;
	float total_period_samples = 0;
	float total_high_time = 0;
	float total_period_time = 0;
	float avg_period_samples = 0;
	float frequency_hz = 0;
	bool last_state = (buffer[0] & 0x01);

	log_inf(PWM_MODULE_NAME, "Iniciando medição de PWM...");

	ana_config_pio_get_data(config);

	for (uint32_t i = 1; i < samples; i++) {
		bool current_state = (buffer[i] & 0x01);

		if (!last_state && current_state) {
			if (rise_count < 1000) {
				rise_edges[rise_count++] = i;
			}
		} else if (last_state && !current_state) {
			if (fall_count < 1000) {
				fall_edges[fall_count++] = i;
			}
		}

		last_state = current_state;
	}

	if (rise_count < 2) {
		log_err(PWM_MODULE_NAME, "Não foram detectadas bordas de subida suficientes");
		return;
	}

	for (uint32_t i = 1; i < rise_count; i++) {
		uint32_t period_samples = rise_edges[i] - rise_edges[i - 1];
		if (period_samples > 0) {
			total_period_samples += period_samples;
			valid_periods++;
		}
	}

	if (valid_periods == 0) {
		log_err(PWM_MODULE_NAME, "Não foi possível calcular o período");
		return;
	}

	avg_period_samples = total_period_samples / valid_periods;
	frequency_hz = sample_rate_hz / avg_period_samples;

	for (uint32_t i = 0; i < rise_count - 1; i++) {
		rise_start = rise_edges[i];
		rise_end = rise_edges[i + 1];

		for (uint32_t j = 0; j < fall_count; j++) {
			if (fall_edges[j] > rise_start && fall_edges[j] < rise_end) {
				high_samples = fall_edges[j] - rise_start;
				period_samples = rise_end - rise_start;
				total_high_time += high_samples;
				total_period_time += period_samples;
				valid_cycles++;
				break;
			}
		}
	}

	if (valid_cycles == 0) {
		log_err(PWM_MODULE_NAME, "Não foi possível calcular o duty cycle");
		return;
	}

	float avg_duty_cycle = (total_high_time / total_period_time) * 100.0f;

	printf("\n=== MEDIÇÃO PWM ===\n");
	printf("Frequência: %.2f Hz\n", frequency_hz);
	printf("Duty Cycle: %.2f%%\n", avg_duty_cycle);
	printf("Período: %.6f segundos\n", 1.0f / frequency_hz);
	printf("Tempo ON: %.6f segundos\n", (1.0f / frequency_hz) * (avg_duty_cycle / 100.0f));
	printf("Tempo OFF: %.6f segundos\n",
	       (1.0f / frequency_hz) * ((100.0f - avg_duty_cycle) / 100.0f));
	printf("Amostras analisadas: %u\n", samples);
	printf("Taxa de amostragem: %.0f Hz\n", sample_rate_hz);
	printf("===================\n");

	log_inf(PWM_MODULE_NAME, "Frequência medida: %.2f Hz", frequency_hz);
	log_inf(PWM_MODULE_NAME, "Duty Cycle medido: %.2f%%", avg_duty_cycle);

	if (config->module.samples > 20) {
		printf("\nPrimeiras 20 amostras:\n");
		for (uint32_t i = 0; i < 20; i++) {
			printf("Amostra[%3u]: %d\n", i, buffer[i] & 0x01);
		}
	}
}

void ana_pwm_set_sample_rate(uint32_t sample_rate_hz)
{
	struct ana_config_system *config = ana_pwm_get_config();
	config->module.sample_rate_hz = sample_rate_hz;

	pio_sm_set_enabled(config->pio.instance, config->pio.sm, false);

	pio_sm_config sm_cfg = config->pio.get_default_cfg_func(config->pio.pio_offset);
	float clkdiv = clock_get_hz(clk_sys) / (float)sample_rate_hz;
	sm_config_set_clkdiv(&sm_cfg, clkdiv);

	sm_config_set_wrap(&sm_cfg, config->pio.pio_offset,
			   config->pio.pio_offset + config->pio.pio_program->length - 1);
	sm_config_set_in_pins(&sm_cfg, config->module.pin_base);
	sm_config_set_in_shift(&sm_cfg, false, true, config->module.pin_count);
	sm_config_set_fifo_join(&sm_cfg, PIO_FIFO_JOIN_RX);

	pio_sm_init(config->pio.instance, config->pio.sm, config->pio.pio_offset, &sm_cfg);

	log_inf(PWM_MODULE_NAME, "Taxa de amostragem configurada para %u Hz", sample_rate_hz);
}

static uint32_t flag = 0;
static void _pwm_signal_handler()
{
	ana_pwm_sm_set_period(&pwm_defs, 1000000);
}

void ana_pwm_generate(void)
{
	char *module_name = "core1 - pwm";
	log_debug(module_name, "Starting PWM generation task");

	while (true) {
		if (multicore_fifo_rvalid()) {
			flag = multicore_fifo_pop_blocking_inline();

			if (flag == PWM_START_FLAG) {
				log_inf(module_name, "Gerando sinal PWM...");
				_pwm_signal_handler();
			} else {
				log_err(module_name, "Flag desconhecida recebida: 0x%08X", flag);
			}
		}
	}
}

void ana_pwm_sm_set_period(struct _pwm_defs *pwm, uint32_t period)
{
	pio_sm_set_enabled(pwm->pio, pwm->sm, false);
	pio_sm_put_blocking(pwm->pio, pwm->sm, period);
	pio_sm_exec(pwm->pio, pwm->sm, pio_encode_pull(false, false));
	pio_sm_exec(pwm->pio, pwm->sm, pio_encode_out(pio_isr, 32));
	pio_sm_set_enabled(pwm->pio, pwm->sm, true);
}

void ana_pwm_sm_set_level(struct _pwm_defs *pwm, uint32_t level)
{
	pio_sm_put_blocking(pwm->pio, pwm->sm, level);
}

struct _pwm_defs* ana_pwm_get_pwm_defs(void)
{
	return &pwm_defs;
}
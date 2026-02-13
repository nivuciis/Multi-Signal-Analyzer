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
#define PWM_TEST_FREQ_HZ  1000u

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

	const uint32_t sample_rate_hz = 1000000; // 1 MHz
	const uint32_t generator_period_cycles = clock_get_hz(clk_sys) / PWM_TEST_FREQ_HZ;


	pwm.pio.instance = pio1;
	pwm.pio.pio_program = &bring_up_pwm_program;
	pwm.pio.get_default_cfg_func = bring_up_pwm_program_get_default_config;

	/**
	 * @note To use wait_for_finish_blocking you can't use a callback
	 *
	 * use NULL for blocking mode
	 */

	/**< pwm.dma_callback = rs232_capture_finished;  */

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
	ana_pwm_sm_set_period(&pwm_defs, generator_period_cycles);
	ana_pwm_sm_set_level(&pwm_defs, generator_period_cycles / 2u);

}

struct ana_config_system *ana_pwm_get_config(void)
{
	return &pwm;
}

void ana_pwm_measure_input_capture(void)
{
    struct ana_config_system *config = ana_pwm_get_config();
    // Importante: Cada medida agora ocupa 1 posição no buffer.
    // buffer[0] = High, buffer[1] = Low
    uint32_t *buffer = (uint32_t *)config->dma.dma_buffer; 
    uint32_t samples = config->module.samples;
    uint32_t sys_clk = clock_get_hz(clk_sys);

    float total_freq = 0;
    float total_duty = 0;
    uint32_t valid_pairs = 0;

    log_inf(PWM_MODULE_NAME, "Processando dados capturados via PIO Hardware...");

    // Dispara a captura DMA e aguarda (assumindo que ana_config_pio_get_data faça isso)
    ana_config_pio_get_data(config); 

    // O PIO envia pares (High e Low). Percorremos de 2 em 2.
    // Usamos samples-1 para garantir que temos o par completo.
    for (uint32_t i = 0; i < samples - 1; i += 2) {
        
        // O contador do PIO é decrescente (0xFFFFFFFF - valor)
        // Multiplicamos por 2 porque o loop no PIO leva 2 ciclos por decremento
        uint32_t high_cycles = (0xFFFFFFFF - buffer[i]) * 2;
        uint32_t low_cycles = (0xFFFFFFFF - buffer[i+1]) * 2;
        
        uint32_t period_cycles = high_cycles + low_cycles;

        if (period_cycles > 0) {
            float freq = (float)sys_clk / period_cycles;
            float duty = ((float)high_cycles / period_cycles) * 100.0f;

            total_freq += freq;
            total_duty += duty;
            valid_pairs++;
        }
    }

    if (valid_pairs == 0) {
        log_err(PWM_MODULE_NAME, "Nenhum ciclo completo capturado");
        return;
    }

    float avg_freq = total_freq / valid_pairs;
    float avg_duty = total_duty / valid_pairs;

    printf("\n=== ANALISADOR PWM PIO (RP2350) ===\n");
    printf("Frequência Média: %.2f Hz\n", avg_freq);
    printf("Duty Cycle Médio: %.2f%%\n", avg_duty);
    printf("Ciclos Analisados: %u\n", valid_pairs);
    printf("Clock do Sistema: %u Hz\n", sys_clk);
    printf("===================================\n");

    log_inf(PWM_MODULE_NAME, "Frequência: %.2f Hz | Duty: %.2f%%", avg_freq, avg_duty);
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
	uint32_t generator_period_cycles = clock_get_hz(clk_sys) / PWM_TEST_FREQ_HZ;
	ana_pwm_sm_set_period(&pwm_defs, generator_period_cycles);
	ana_pwm_sm_set_level(&pwm_defs, generator_period_cycles / 2u);
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
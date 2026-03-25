/** -------------------------------------------------------------
 * @file capture_data.c
 * @brief Capture data module implementation
 *
 * Contrato com o PIO (novo em v1.0)
 * ==================================
 * Todos os programas capture_prog_* agora controlam a contagem de
 * amostras internamente via registrador X:
 *
 *   pull            ; lê (samples - 1) do TX FIFO → OSR → X
 *   mov x, osr
 *   ...             ; loop de captura com jmp x--
 *   irq 0           ; sinaliza fim ao firmware
 *   jmp LOCK        ; bloqueia o SM → DMA para naturalmente
 *
 * Por isso, antes de habilitar o SM, este módulo injeta o valor
 * (cfg->samples - 1) no TX FIFO via pio_sm_put_blocking().
 * Sem esse PULL o SM trava imediatamente na instrução pull.
 *
 * Sequência de start (ana_capture_data_start):
 *   1. ana_capture_init()          — zera buffer, snapshot de cfg->samples
 *   2. DMA configurado e armado    — ainda não iniciado
 *   3. pio_sm_put_blocking()       — injeta contador no TX FIFO
 *   4. dma_channel_start()         — ativa DMA
 *   5. pio_sm_set_enabled()        — PIO começa: faz PULL, configura X, roda
 *   6. Aguarda IRQ 0 do PIO        — captura concluída de forma determinística
 *   7. dma_channel_wait_for_finish — garante que todos os bytes saíram do FIFO
 *
 * @author   Vinicius Rafael Marques de Carvalho <vinicius.carvalho@edge.ufal.br>
 * @author   João Matheus Nascimento Dias <joao.dias@edge.ufal.br>
 * @version  1.0
 * @date     27/04/2026
 * @copyright Copyright (c) 2026
 *  ------------------------------------------------------------*/

#include "capture_data.h"
#include "module.h"

#include <stdint.h>
#include <string.h>

#include <hardware/dma.h>
#include <hardware/irq.h>
#include <hardware/pio.h>
#include <pico/error.h>

#define ANALOG_CHANNEL_SIZE 3

/* Snapshot de cfg->samples tirado no início de cada captura.
 * Evita TOCTOU caso o host mande um novo 'L' durante a captura. */
static uint32_t capture_samples_snapshot;

/* ─────────────────────────────────────────────────────────────────────────
 * IRQ handler do PIO — sinaliza fim de captura via flag no DMA config
 *
 * O PIO dispara IRQ 0 ao terminar as N amostras. Limpamos o flag de
 * IRQ e desabilitamos o handler — o DMA drena o que sobrou no FIFO
 * e para sozinho (SM travado em LOCK).
 * ───────────────────────────────────────────────────────────────────────── */
static volatile bool pio_capture_done = false;

static void pio_irq_handler(void)
{
	/* Limpa o flag de IRQ 0 de qualquer SM no PIO0/PIO1.
	 * O SDK não expõe diretamente qual SM gerou; limpamos o bit 0
	 * (pio_interrupt_clear usa índice 0–7). */
	struct ana_module_system *config = ana_channels_get_module();
	pio_interrupt_clear(config->pio.instance, 0);
	pio_capture_done = true;
}

/* ─────────────────────────────────────────────────────────────────────────
 * ana_capture_init
 * ───────────────────────────────────────────────────────────────────────── */
int ana_capture_init(struct ana_module_system *config)
{
	if (ana_module_pio_dma_is_busy(config)) {
		ana_module_pio_dma_abort(config);
	}

	/* Snapshot atômico do número de amostras — usado em todo este ciclo */
	struct pulseview_sample_config *cfg = ana_sigrok_get_sample_config();
	capture_samples_snapshot = cfg->samples;

	memset(config->dma.dma_buffer, 0, capture_samples_snapshot * sizeof(uint16_t));

	pio_capture_done = false;

	return PICO_OK;
}

/* ─────────────────────────────────────────────────────────────────────────
 * ana_capture_data_get_analog_channels_count
 * ───────────────────────────────────────────────────────────────────────── */
int ana_capture_data_get_analog_channels_count(uint8_t analog_mask)
{
	int count = 0;
	for (int i = 0; i < ANALOG_CHANNEL_SIZE; i++) {
		if (analog_mask & (1u << i)) {
			count++;
		}
	}
	return count;
}

/* ─────────────────────────────────────────────────────────────────────────
 * ana_capture_data_start
 *
 * Executa uma captura completa de forma bloqueante.
 * Usa o snapshot tirado em ana_capture_init para garantir consistência.
 * ───────────────────────────────────────────────────────────────────────── */
void ana_capture_data_start(struct ana_module_system *config)
{
	/* Passo 1: zera buffer e tira snapshot de samples */
	ana_capture_init(config);

	PIO pio       = config->pio.instance;
	uint sm       = config->pio.sm;
	uint dma_ch   = config->dma.instance;
	uint32_t n    = capture_samples_snapshot;

	/* Passo 2: configura o DMA (destino, fonte, contagem) mas NÃO inicia */
	dma_channel_configure(
		dma_ch,
		&config->dma.instance_cfg,
		config->dma.dma_buffer,          /* destino: buffer de captura  */
		&pio->rxf[sm],                    /* fonte: RX FIFO do SM        */
		n,                                /* N transferências de 16 bits  */
		false                             /* não inicia ainda             */
	);

	/* Passo 3: registra IRQ do PIO para detectar fim de captura */
	pio_set_irq0_source_enabled(pio, pis_interrupt0, true);

	uint pio_irq = (pio == pio0) ? PIO0_IRQ_0 : PIO1_IRQ_0;
	irq_set_exclusive_handler(pio_irq, pio_irq_handler);
	irq_set_enabled(pio_irq, true);

	/* Passo 4: injeta o contador no TX FIFO ANTES de habilitar o SM.
	 * O PIO faz PULL desse valor para X na primeira instrução.
	 * Se samples == 0 nunca deve chegar aqui, mas protegemos com
	 * um mínimo de 1. */
	uint32_t counter = (n > 0) ? (n - 1) : 0;
	pio_sm_put_blocking(pio, sm, counter);

	/* Passo 5: inicia DMA e habilita SM — ordem importa:
	 * DMA deve estar pronto antes do SM começar a empurrar dados */
	dma_channel_start(dma_ch);
	pio_sm_set_enabled(pio, sm, true);

	/* Passo 6: aguarda sinal do PIO (IRQ 0) — captura determinística */
	while (!pio_capture_done) {
		tight_loop_contents();
	}

	/* Desabilita IRQ do PIO — não precisamos mais */
	irq_set_enabled(pio_irq, false);
	irq_remove_handler(pio_irq, pio_irq_handler);
	pio_set_irq0_source_enabled(pio, pis_interrupt0, false);

	/* Passo 7: drena o que pode ter sobrado no FIFO (últimos bytes) */
	dma_channel_wait_for_finish_blocking(dma_ch);

	/* Desabilita o SM — estará travado em LOCK, mas garantimos o estado */
	pio_sm_set_enabled(pio, sm, false);

	config->dma.has_complete = true;
}

/** -------------------------------------------------------------
 * @file capture_data.h
 * @brief Capture data module interface
 *
 * @author   Vinicius Rafael Marques de Carvalho <vinicius.carvalho@edge.ufal.br>
 * @author   João Matheus Nascimento Dias <joao.dias@edge.ufal.br>
 * @version  1.0
 * @date     27/04/2026
 * @copyright Copyright (c) 2026
 *  ------------------------------------------------------------*/

#ifndef CAPTURE_DATA_H
#define CAPTURE_DATA_H

#include "module.h"

#include <stdint.h>

/**
 * @brief Prepara o sistema para uma nova captura.
 *
 * - Aborta qualquer DMA em andamento
 * - Tira snapshot de cfg->samples (imune a mudanças do host durante a captura)
 * - Zera o DMA buffer
 * - Reseta a flag interna de conclusão do PIO
 *
 * @param config Ponteiro para a configuração do módulo
 * @return PICO_OK em sucesso
 */
int ana_capture_init(struct ana_module_system *config);

/**
 * @brief Executa uma captura completa de forma bloqueante.
 *
 * Sequência interna:
 *   1. ana_capture_init() — zera estado
 *   2. DMA configurado e armado (sem iniciar)
 *   3. IRQ 0 do PIO habilitado para detectar fim de captura
 *   4. pio_sm_put_blocking(samples - 1) — injeta contador no TX FIFO
 *   5. DMA iniciado → SM habilitado
 *   6. Aguarda IRQ 0 do PIO (sinal de N amostras capturadas)
 *   7. dma_channel_wait_for_finish — drena FIFO residual
 *
 * Retorna somente após todas as amostras terem sido gravadas no buffer.
 *
 * @param config Ponteiro para a configuração do módulo
 */
void ana_capture_data_start(struct ana_module_system *config);

/**
 * @brief Retorna a contagem de canais analógicos habilitados.
 *
 * @param analog_mask Bitmask dos canais analógicos habilitados
 * @return int Número de canais ativos (0–3)
 */
int ana_capture_data_get_analog_channels_count(uint8_t analog_mask);

#endif /* CAPTURE_DATA_H */

/*******************************************************************
 * @file channels.h
 *
 * @brief Channel control for the Multi-Signal Analyzer project
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 09/03/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#ifndef CHANNELS_H
#define CHANNELS_H

#include "module.h"

/**
 * @brief Initialize the channels module
 *
 */
void ana_channels_init(PIO pio);

/**
 * @brief Get the module configuration for the channels
 *
 * @return struct ana_module_system* Pointer to the module configuration structure
 */
struct ana_module_system *ana_channels_get_module(void);

/**
 * @brief Return the DMA buffer not currently pointed to by the module's dma_buffer.
 *
 * Used for double-buffering: while DMA fills one buffer the other is free for TX.
 */
uint16_t *ana_channels_get_alt_buffer(void);

#endif /* CHANNELS_H */

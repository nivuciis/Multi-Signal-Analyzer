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
struct ana_module_system* ana_channels_get_module(void);

/**
 * @brief Apply the current trigger configuration to the PIO program.
 *
 * Must be called before each capture. Reloads the PIO state machine with
 * the program matching the trigger type stored in the sigrok handler.
 */
void ana_channels_apply_trigger(void);

/**
 * @brief Return the DMA buffer not currently pointed to by the module's dma_buffer.
 *
 * Used for double-buffering: while DMA fills one buffer the other is free for TX.
 */
uint16_t *ana_channels_get_alt_buffer(void);

/**
 * @brief Reload PIO with the simple (no-trigger) capture program.
 *
 * Call after the first triggered chunk has completed so that subsequent chunks
 * do not re-arm the trigger and wait again instead of capturing continuously.
 */
void ana_channels_load_simple(void);

#endif /* CHANNELS_H */

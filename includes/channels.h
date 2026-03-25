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

#endif /* CHANNELS_H */

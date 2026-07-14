/*******************************************************************
 * @file rs485.h
 *
 * @brief RS485 channel control for the Multi-Signal Analyzer project
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 25/05/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#ifndef RS485_H
#define RS485_H

#include "module.h"

/**
 * @brief Initialize the RS485 module
 */
void ana_rs485_init(void);

/**
 * @brief Get the module configuration for the RS485 channel
 *
 * @return struct ana_module_system* Pointer to the module configuration structure
 */
struct ana_module_system *ana_rs485_get_module(void);

/**
 * @brief Return the capture buffer not currently pointed to by the module.
 *
 * Used for double-buffering: while one buffer is being filled the other is free for TX.
 */
uint16_t *ana_rs485_get_alt_buffer(void);

#endif /* RS485_H */

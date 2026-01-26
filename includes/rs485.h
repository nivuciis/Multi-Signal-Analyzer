/*******************************************************************
 * @file rs485.h
 *
 * @brief RS485 communication commands test
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 08/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/
#ifndef RS485_H
#define RS485_H

#include "config_pio.h"

#include <stdint.h>

/**
 * @brief Initialize the GPIOs module.
 *
 */
void ana_rs485_init(void);

/**
 * @brief Get data configuration of RS485.
 * 
 * @return (struct ana_config_system*) Point to Configuration of RS485 module
 */
struct ana_config_system *ana_rs485_get_config(void);

#endif /* RS485_H */

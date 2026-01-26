/*******************************************************************
 * @file rs232.h
 *
 * @brief RS232 communication commands test
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 08/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/
#ifndef RS232_H
#define RS232_H

#include "config_pio.h"

#include <stdint.h>

/**
 * @brief Initialize the GPIOs module.
 *
 */
void ana_rs232_init(void);

/**
 * @brief Get data configuration of RS232.
 * 
 * @return (struct ana_config_pio*) Point to Configuration of RS232 module
 */
struct ana_config_system *ana_rs232_get_config(void);


#endif /* RS232_H */

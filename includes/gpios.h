/*******************************************************************
 * @file gpios.h
 *
 * @brief GPIOS definition and initialization
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 08/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/
#ifndef GPIOS_H
#define GPIOS_H

#include "config_pio.h"

#include <stdint.h>

/**
 * @brief Initialize the GPIOs module.
 *
 */
void ana_gpios_init(void);

/**
 * @brief Get data configuration of GPIOS.
 *
 * @return (struct ana_config_system *) Point to Configuration of GPIOs module
 */
struct ana_config_system *ana_gpios_get_config(void);

#endif /* GPIOS_H */
    
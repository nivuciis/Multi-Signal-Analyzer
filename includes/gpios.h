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
 * @brief Number of GPIO pins used.
 *
 */
#define GPIOS_NUM_PINS 12

/**
 * @brief Starting GPIO pin number.
 *
 */
#define GPIOS_START_PIN 9

/**
 * @brief Initialize the GPIOs module.
 *
 */
void ana_gpios_init(void);

/**
 * @brief Get data from GPIOs.
 *
 */
void ana_gpios_get_data(void);

/**
 * @brief Get data from GPIOs (version 2).
 *
 */
void ana_gpios_get_data_v2(void);

/**
 * @brief Print GPIO data.
 *
 */
void ana_gpios_print_data(void);

/**
 * @brief Test PIO direct functionality.
 *
 */
void ana_gpios_test_pio_direct(void);

/**
 * @brief Get buffer of GPIO data.
 *
 */
uint16_t *ana_gpios_get_buffer(void);

#endif /* GPIOS_H */

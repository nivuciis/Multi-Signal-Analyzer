/*******************************************************************
 * @file can.h
 *
 * @brief CAN communication commands test
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 08/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/
#ifndef CAN_H
#define CAN_H

#include "config_pio.h"

#include <stdint.h>

/**
 * @brief Initialize the GPIOs module.
 *
 */
void ana_can_init(void);

/**
 * @brief Get data configuration of CAN.
 * 
 * @return (struct ana_config_system*) Point to Configuration of CAN module 
 */
struct ana_config_system *ana_can_get_config(void);

#endif /* CAN_H */

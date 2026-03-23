/** -------------------------------------------------------------
 * @file capture_data.h
 * @brief Capture data module interface
 *
 * @author   Vinicius Rafael Marques de Carvalho <vinicius.carvalho@edge.ufal.br>
 * @author   João Matheus Nascimento Dias <joao.dias@edge.ufal.br>
 * @version   v1.0
 * @date      17/03/2026
 * @copyright   Copyright (c) 2026
 *  ------------------------------------------------------------*/

#ifndef CAPTURE_DATA_H
#define CAPTURE_DATA_H
#include "module.h"

/**
 * @brief Configure system to capture data
 */
int ana_capture_init(struct ana_module_system *config);

/**
 * @brief Start the capture processing
 *
 * @param config Module configuration structure
 */
void ana_capture_data_start(struct ana_module_system *config);

/**
 * @brief Get the analog channels count
 *
 * @param config Pointer to the module configuration
 * @param analog_mask bitmask of analog channels enabled
 * @return int number of analog channels enabled
 */
int ana_capture_data_get_analog_channels_count(uint8_t analog_mask);

#endif // CAPTURE_DATA_H
/** -------------------------------------------------------------
 * @file capture_data.h
 * @brief Capture data module interface
 *
 * @author    Vinicius Rafael Marques de Carvalho vinicius.carvalho@edge.ufal.br
 * @version   v1.0
 * @date      28/11/2025
 * @copyright
 *  ------------------------------------------------------------*/

#ifndef CAPTURE_DATA_H
#define CAPTURE_DATA_H

#include "pico/stdlib.h"

/**
 * @brief Initate the PIO and DMA
 */
int capture_init();

/**
 * @brief Start the capturing process
 *  @param sample_count Number of samples to capture
 * @param sample_rate Sample rate in Hz
 */
void capture_data(uint32_t sample_count, uint32_t sample_rate);

#endif // CAPTURE_DATA_H

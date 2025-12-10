/** -------------------------------------------------------------
 * @file capture_data.h
 * @brief Capture data module interface
 *
 * @author    Vinicius Rafael Marques de Carvalho <vinicius.carvalho@edge.ufal.br>
 * @version   v1.0
 * @date      05/12/2025
 * @copyright
 *  ------------------------------------------------------------*/

#ifndef CAPTURE_DATA_H
#define CAPTURE_DATA_H

#define CAPTURE_BUFFER_SIZE (16 * 1024)

#include <pico/stdlib.h>

/**
 * @brief Initate the PIO and DMA
 */
int capture_init(void);

/**
 * @brief Send the buffer through USB
 *
 * @param buffer Buffer that stores information from the PIO state machine
 * @param num_samples Number of samples captured by the PIO program
 */
void send_captured_data(const uint32_t *buffer, uint32_t num_samples);

/**
 * @brief Start the capturing process
 *
 * @param sample_count Number of samples to capture
 * @param sample_rate_hz Sample rate in Hz
 */
void capture_data(uint32_t sample_count, uint32_t sample_rate_hz, uint32_t *capture_buffer);

#endif // CAPTURE_DATA_H

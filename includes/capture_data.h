/** -------------------------------------------------------------
 * @file capture_data.h
 * @brief Capture data module interface
 *
 * @author    Vinicius Rafael Marques de Carvalho <vinicius.carvalho@edge.ufal.br>
 * @author   João Matheus Nascimento Dias <joao.dias@edge.ufal.br>
 * @version   v1.0
 * @date      15/01/2026
 * @copyright   Copyright (c) 2026
 *  ------------------------------------------------------------*/

#ifndef CAPTURE_DATA_H
#define CAPTURE_DATA_H

#include <pico/stdlib.h>

/**
 * @brief Define the length of the capture buffers
 *
 */
#define CAPTURE_DEPTH       4096
#define CAPTURE_BUFFER_SIZE (16 * 1024)

/**
 * @brief Digital buffer
 * @note Captures 32 bits per sample
 */
extern uint32_t digital_capture_buffer[CAPTURE_DEPTH];

/**
 * @brief Analog buffer
 * @note Captures 3 bytes per sample
 */
extern uint8_t analog_capture_buffer[CAPTURE_DEPTH * 3];

/**
 * @brief Initate the PIO and DMA
 */
int ana_capture_init(void);

/**
 * @brief Send the buffer through USB
 *
 * @param buffer Buffer that stores information from the PIO state machine
 * @param num_samples Number of samples captured by the PIO program
 */
void ana_send_captured_data(const uint32_t *buffer, uint32_t num_samples);

/**
 * @brief Start the capturing process
 *
 * @param sample_count Number of samples to capture
 * @param sample_rate_hz Sample rate in Hz
 */
void ana_capture_data(uint32_t sample_count, uint32_t sample_rate_hz, uint32_t *capture_buffer);

#endif // CAPTURE_DATA_H

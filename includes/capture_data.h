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
#define CAPTURE_DEPTH 4096

/**
 * @brief Define the size of the capture buffers
 *
 */
#define CAPTURE_BUFFER_SIZE (16 * 1024)

/**
 * @brief Set the base pin for digital capture
 *
 * @note 16 consecutive pins will be used starting from BASE_PIN
 */

#define BASE_PIN 0

/**
 * @brief Digital buffer
 */
extern uint16_t digital_capture_buffer[CAPTURE_DEPTH];

/**
 * @brief Analog buffer
 */
extern uint8_t analog_capture_buffer[CAPTURE_DEPTH * 3];

/**
 * @brief Initialize the GPIO`s, PIO and DMA for data capture
 */
int ana_capture_init(void);

/**
 * @brief Start the capture processing
 *
 * @param sample_count Number of samples to capture
 * @param sample_rate_hz Sample rate in Hz
 * @param analog_mask Bitmask to indicate which analog channels to capture
 */
void ana_capture_data(uint32_t sample_count, uint32_t sample_rate_hz, uint8_t analog_mask);

/**
 * @brief Get the analog channels count
 *
 * @param analog_mask bitmask of analog channels enabled
 * @return int number of analog channels enabled
 */
int ana_get_analog_channels_count(uint8_t analog_mask);

#endif // CAPTURE_DATA_H

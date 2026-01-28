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
extern uint16_t digital_capture_buffer[CAPTURE_DEPTH];

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
 * @brief Start the capturing process
 *
 * @param sample_count Number of samples to capture
 * @param sample_rate_hz Sample rate in Hz
 * @param capture_buffer Pointer to the buffer that will store the captured data
 * @param analog_mask Bitmask to indicate which analog channels to capture
 */
void ana_capture_data(uint32_t sample_count, uint32_t sample_rate_hz, uint32_t *capture_buffer, uint8_t analog_mask);


/**
 * @brief Get the analog channels count object
 * 
 * @param analog_mask bitmask of analog channels enabled
 * @return int number of analog channels enabled
 */
int ana_get_analog_channels_count(uint8_t analog_mask);

#endif // CAPTURE_DATA_H

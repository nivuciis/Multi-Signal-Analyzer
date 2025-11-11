#ifndef CAPTURE_DATA_H
#define CAPTURE_DATA_H

#include "pico/stdlib.h"

/**
 * @brief Initate the PIO and DMA 
 */
void capture_init();

/**
 * @brief Arm and start the capturing process 
 * Configure PIO and DMA 
 * Blocks until finished and then send data through USB
 * * @param sample_count Number of samples to capture (
 * @param sample_rate Sample rate in Hz
 */
void capture_arm_and_send(uint32_t sample_count, uint32_t sample_rate);

#endif // CAPTURE_DATA_H
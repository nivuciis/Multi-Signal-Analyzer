#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 
 * 
 * @param buffer 
 * @param num_samples 
 */
void send_capture_data(const uint32_t *buffer, uint32_t num_samples);

/**
 * @brief 
 * 
 * @param buffer 
 * @param src 
 */
void print_buffer(uint32_t* buffer);

/**
 * @brief 
 * 
 * @param buffer 
 * @param len 
 * @return true 
 * @return false 
 */
bool read_serial(uint8_t *buffer, size_t len);


#endif /* UTIL_H */
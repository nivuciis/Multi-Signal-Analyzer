/** -------------------------------------------------------------
 * @file sigrok_handler.h
 * @brief 
 *
 * @author   Vinicius Rafael Marques de Carvalho <vinicius.carvalho@edge.ufal.br>
 * @author   João Matheus Nascimento Dias <joao.dias@edge.ufal.br>
 * @version  0.1
 * @date     15/01/2026
 * @copyright Copyright (c) 2026
 *  ------------------------------------------------------------*/


#ifndef SIGROK_HANDLER_H
#define SIGROK_HANDLER_H
#include <stdint.h>

/**
 * @brief Identifier 
 * 
 * @note its needed to identify the device to Sigrok/PulseView 
 */
#define IDENTIFY_CMD 'i'

/**
 * @brief Set sample rate 
 * 
 * @note Sets the sample rate for the capture
*/
#define SET_SAMPLE_RATE_CMD 'R'

/**
 * @brief Set sample limit 
 * 
 * @note Sets the sample limit for the capture
 */
#define SET_SAMPLE_LIMIT_CMD 'L'

/**
 * @brief Get analog scale 
 * 
 * @note Gets the analog scale for the capture
 */
#define GET_ANALOG_SCALE_CMD 'a'

/**
 * @brief enable analog channel 
 * 
 */
#define ENABLE_ANALOG_CHANNEL_CMD 'A'

/**
 * @brief Enable digital channel 
 * 
 */
#define ENABLE_DIGITAL_CHANNEL_CMD 'D'

/**
 * @brief Starts the capture process 
 * 
 */
#define FIXED_CAPTURE_CMD 'F'

/**
 * @brief Initialize the Sigrok protocol handler
 */
void sigrok_init(void);

/**
 * @brief Process the received messages from Sigrok/PulseView
 * 
 * @param received_message The received command byte
 */
void sigrok_process_byte(uint8_t received_command);

#endif 
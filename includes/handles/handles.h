/*******************************************************************
 * @file handles.h
 *
 * @brief Public header for handle functions.
 *
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 12/05/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#ifndef HANDLES_H
#define HANDLES_H

/**
 * @brief Handles the identify command
 * 
 */
void handle_identify(void);

/**
 * @brief Handles setting the sample rate
 * 
 */
void handle_set_sample_rate(void);

/**
 * @brief Handles setting the sample limit
 * 
 */
void handle_set_sample_limit(void);

/**
 * @brief Handles getting the analog scale
 * 
 */
void handle_get_analog_scale(void);

/**
 * @brief Handles setting the analog channel
 * 
 */
void handle_set_analog_channel(void);

/**
 * @brief Handles setting the digital channel
 * 
 */
void handle_set_digital_channel(void);

/**
 * @brief Handles fixed capture
 *  
 */
void handle_fixed_capture(void);

/**
 * @brief Handles continuous capture
 * 
 */
void handle_continuous_capture(void);

/**
 * @brief Handles setting the pretrigger
 * 
 */
void handle_set_pretrigger(void);

/**
 * @brief Handles setting the trigger
 * 
 */
void handle_set_trigger(void);

#endif /* HANDLES_H */

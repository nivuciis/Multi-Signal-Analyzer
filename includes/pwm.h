/*******************************************************************
 * @file pwm.h
 *
 * @brief PWM module test
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 28/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/

#ifndef PWM_H
#define PWM_H

#include "config_pio.h"

/**
 * @brief Initialize the PWM module test.
 *
 */
void ana_pwm_init(void);

/**
 * @brief Get data configuration of PWM.
 *
 * @return (struct ana_config_pio*) Point to Configuration of PWM module
 */
struct ana_config_system *ana_pwm_get_config(void);

/**
 * @brief Measure PWM using Input Capture.
 *
 */
void ana_pwm_measure_input_capture(void);

/**
 * @brief Set the sample rate for PWM measurement.
 * 
 * @param sample_rate_hz The desired sample rate in Hertz.
 */
void ana_pwm_set_sample_rate(uint32_t sample_rate_hz);

#endif /* PWM_H */

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
#include <pico/types.h>

/**
 * @brief Flag to indicate the start of PWM signal generation at core 1.
 *
 */
#define PWM_START_FLAG 0xAAAAAAAA

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

/**
 * @note: This function is running at core 1
 * and wait for a flag from core 0 to start the PWM generation.
 * All this part is dedicated to generate a PWM signal for testing purposes.
 *
 */

 /**
  * @brief PWM definitions structure
  * 
  */
struct _pwm_defs {
    PIO pio;
    uint sm;
    uint offset;
};

/**
 * @brief Generate PWM signal for testing.
 * 
 */
void ana_pwm_generate();

/**
 * @brief Set the PWM period for the state machine.
 * 
 * @param pwm Pointer to PWM definitions structure 
 * @param period The desired PWM period in samples
 */
void ana_pwm_sm_set_period(struct _pwm_defs *pwm, uint32_t period);

/**
 * @brief Set the PWM high level for the state machine.
 * 
 * @param pwm Pointer to PWM definitions structure 
 * @param level The desired PWM high level in samples
 */
void ana_pwm_sm_set_level(struct _pwm_defs *pwm, uint32_t level);

/**
 * @brief Get the PWM definitions structure.
 * 
 * @return struct _pwm_defs* Pointer to the PWM definitions structure
 */
struct _pwm_defs* ana_pwm_get_pwm_defs(void);



#endif /* PWM_H */

/*******************************************************************
 * @file adc.h
 *
 * @brief ADC configuration and control interface for analog channels
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 23/03/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#ifndef ADC_H
#define ADC_H

#include "module.h"

#include <hardware/adc.h>
#include <hardware/gpio.h>

/**
 * @brief
 *
 */
struct buffs {
	double *chan1; /**< Buffer to analogic channel 1 */
	double *chan2; /**< Buffer to analogic channel 2 */
	double *chan3; /**< Buffer to analogic channel 3 */
};

/**
 * @brief Configuration structure for the ADC module
 *
 */
struct ana_adc_module {
	struct ana_module_config const module; /**< Module configuration */
	float clkdiv;                          /**< ADC clock divider for sampling rate control */
	struct buffs buffers;                  /**< Analogics channels buffers */
};

/**
 * @brief Initialize the ADC for analog channel reading
 *
 */
void ana_adc_init(void);

/**
 * @brief Set the ADC clock divider to adjust the sampling rate
 *
 * @param clkdiv The clock divider value
 * @note 1.0 for no division
 */
void ana_adc_set_clkdiv(float clkdiv);

/**
 * @brief Read the ADC value from a specified channel and store it in the provided pointer
 *
 * @param rsp  Pointer to a buffer where the ADC result will be stored
 * @param rsp2 Pointer to a buffer where the ADC result for the second channel will be stored
 * (optional)
 * @param rsp3 Pointer to a buffer where the ADC result for the third channel will be stored
 * (optional)
 */
void ana_adc_set_buffers(double *rsp, double *rsp2, double *rsp3);

/**
 * @brief Read the ADC value from a specified channel and return it as a float
 *
 * @param rsp Pointer to a double variable where the ADC result will be stored
 * @param channel The ADC channel number to read from
 */
void ana_adc_read(double *rsp, uint8_t channel);

/**
 * @brief Read the ADC values from multiple channels and store them in the provided buffers
 *
 * @param adc The ADC module configuration
 * @param samples The number of samples to read
 */
void ana_adc_read_all(uint32_t samples);

/**
 * @brief  Get the pointers to the ADC result buffers for all channels
 *
 * @return struct buffs* Pointer to the structure containing the ADC result buffers
 */
struct buffs *ana_adc_get_buffs();

#endif /* ADC_H */

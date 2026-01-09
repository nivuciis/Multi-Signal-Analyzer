/*******************************************************************
 * @file adc.h
 *
 * @brief Led control commands
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 08/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/
#ifndef ADC_H
#define ADC_H

#include <stdint.h>

#include <pico/types.h>
#include <hardware/gpio.h>
#include <hardware/pio.h>

static PIO pio_adc = pio1;
static uint sm_adc = 2;
static uint pio_adc_offset;
static int dma_adc_chan;

/**
 * @brief Number of ADC channels
 * 
 */
#define ADC_NUM_CHANNELS 3

/**
 * @brief GPIO pin for ADC associated to channel 1
 * 
 */
#define ADC_CHAN1_GPIO_PIN 47

/**
 * @brief GPIO pin for ADC associated to channel 2
 * 
 */
#define ADC_CHAN2_GPIO_PIN 46

/**
 * @brief GPIO pin for ADC associated to channel 3
 * 
 */
#define ADC_CHAN3_GPIO_PIN 45

/**
 * @brief Initialize the ADC system.
 * 
 */
void ana_adc_init(void);

/**
 * @brief Set the clock divider for the ADC.
 * 
 * @param clkdiv The clock divider value to set.
 */
void ana_adc_set_clkdiv(float clkdiv);

/**
 * @brief Read a single ADC channel.
 * 
 * @param rsp Pointer to store the ADC reading result.
 * @param gpio_pin GPIO pin associated with the ADC channel.
 */
void read_adc_single_channel(double *rsp, int gpio_pin);

/**
 * @brief Read multiple ADC channels.
 * 
 * @param rsp1 Pointer to store the ADC reading result for channel 1.
 * @param rsp2 Pointer to store the ADC reading result for channel 2.
 * @param rsp3 Pointer to store the ADC reading result for channel 3.
 * @param samples Number of samples to read.
 */
void read_adc_multiple_channels(double *rsp1, double *rsp2, double *rsp3, uint16_t samples);



#endif /* ADC_H */
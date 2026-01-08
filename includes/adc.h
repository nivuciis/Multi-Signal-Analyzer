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
 * @brief Read the values from the ADC channels.
 * 
 * @param chan1 Pointer to store the value from channel 1
 * @param chan2 Pointer to store the value from channel 2
 * @param chan3 Pointer to store the value from channel 3
 */
void ana_adc_read_channels(uint16_t* chan1, uint16_t* chan2, uint16_t* chan3);


#endif /* ADC_H */
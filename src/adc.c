/*******************************************************************
 * @file adc.c
 *
 * @brief Led control commands
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 08/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/
#include "adc.h"

#include <assert.h>
#include <stdbool.h>

#include <cstdint>
#include <hardware/adc.h>
#include <hardware/gpio.h>
#include <hardware/pio.h>

/**
 * @brief Base GPIO pin number for ADC channels in Subtravtic board.
 * @note ADC channels are mapped to GPIO pins starting from this base.
 *
 * - At projet we are using the following mapping:
 * => GPIO 45 = chan3
 * => GPIO 46 = chan2
 * => GPIO 47 = chan1
 * => mask input: 0b0000_0111 = 0x07
 *
 * - At the doc of RP PICO 2 SDK, the ADC channels are mapped as follows:
 * - input adc 0 --> GPIO 40
 * - input adc 1 --> GPIO 41
 * - input adc 2 --> GPIO 42
 * - input adc 3 --> GPIO 43
 * - input adc 4 --> GPIO 44
 * - input adc 5 --> GPIO 45
 * - input adc 6 --> GPIO 46
 * - input adc 7 --> GPIO 47
 */
#define ADC_BASE_SUBTRAVTIC 40

/**
 * @brief Mask for ADC channels reading.
 *
 */
#define ADC_CHANNELS_MASK 0x07

/**
 * @brief Conversion factor for ADC readings to voltage.
 *
 * @note At the project the power of ADC CI is 3v3 and it has 12 bits of resolution.
 * Thus, the conversion factor is calculated as 3.3 / (2^12).
 */
static const double ADC_FACTOR_CONVERSION = 3.3f / (double)(1 << 12);

void read_adc_single_channel(double *rsp, int gpio_pin)
{
	uint16_t raw_data = 0;
	double result = 0.0f;

	adc_select_input(gpio_pin - ADC_BASE_SUBTRAVTIC);

	raw_data = adc_read();
	result = (double)raw_data * ADC_FACTOR_CONVERSION *
		 1000.0f; /* The value returned is in millivolts */
	*rsp = result;
}

void read_adc_multiple_channels(double *rsp1, double *rsp2, double *rsp3, uint16_t samples)
{
	samples = (samples <= 0) ? 1 : samples;

	uint16_t raw_data = 0;

	adc_run(true);

	for (uint16_t i = 0; i < samples; i++) {
		read_adc_single_channel(rsp1 + i, ADC_CHAN1_GPIO_PIN);
		read_adc_single_channel(rsp2 + i, ADC_CHAN2_GPIO_PIN);
		read_adc_single_channel(rsp3 + i, ADC_CHAN3_GPIO_PIN);
	}

	adc_run(false);
}

void ana_adc_init(void)
{
	adc_init();

	adc_gpio_init(ADC_CHAN1_GPIO_PIN);
	adc_gpio_init(ADC_CHAN2_GPIO_PIN);
	adc_gpio_init(ADC_CHAN3_GPIO_PIN);

	adc_run(false);
}

void ana_adc_set_clkdiv(float clkdiv)
{
	adc_set_clkdiv(clkdiv);
}

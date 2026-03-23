/*******************************************************************
 * @file adc.c
 *
 * @brief ADC implementation program to read analog channels
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 23/03/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#include "adc.h"
#include "log.h"

#include <assert.h>

#include <hardware/adc.h>
#include <pico/types.h>

#define ADC_BASE_SUBTRACTIV 40

#if PICO_DEFAULT_ADC_VOLTAGE_DIVIDER
static const double ADC_VOLTAGE_DIVIDER_R1 = 300000.0; /* Resistor R1 value in ohms */
static const double ADC_VOLTAGE_DIVIDER_R2 = 100000.0; /* Resistor R2 value in ohms */
#else
static const double ADC_VOLTAGE_DIVIDER_R1 = 0.0; /* Resistor R1 value in ohms */
static const double ADC_VOLTAGE_DIVIDER_R2 = 1.0; /* Resistor R2 value in ohms */
#endif

#define ADC_VOLTAGE_DIVIDER_FACTOR                                                                 \
	((ADC_VOLTAGE_DIVIDER_R1 + ADC_VOLTAGE_DIVIDER_R2) / ADC_VOLTAGE_DIVIDER_R2)

#define ADC_CHECK_CHAN_BUF(buf)                                                                    \
	do {                                                                                       \
		assert(buf != NULL);                                                               \
                                                                                                   \
	} while (0)

static const double ADC_FACTOR_CONVERSION = (3.3f / (double)(1 << 12)) * ADC_VOLTAGE_DIVIDER_FACTOR;

struct ana_adc_module self = {
	.module =
		{
			.name = "ADC",
			.pin_base = PICO_DEFAULT_ADC_PIN_BASE,
			.pin_count = PICO_DEFAULT_ADC_PIN_COUNT,
			.mask = 0x000F,
		},
	.clkdiv = 0.0f,
	.buffers =
		{
			.chan1 = NULL,
			.chan2 = NULL,
			.chan3 = NULL,
		},
};

static uint16_t raw_data = 0;
static double result = 0.0;

void ana_adc_init()
{
	log_debug(self.module.name, "Initializing ADC ....");

	adc_init();

	for (int i = 0; i < self.module.pin_count; i++) {
		adc_gpio_init(self.module.pin_base + i);
	}

	adc_run(false);
}

void ana_adc_set_clkdiv(float clkdiv)
{
	self.clkdiv = clkdiv;
	adc_set_clkdiv(clkdiv);
}

void ana_adc_set_buffers(double *rsp, double *rsp2, double *rsp3)
{
	ADC_CHECK_CHAN_BUF(rsp);
	ADC_CHECK_CHAN_BUF(rsp2);
	ADC_CHECK_CHAN_BUF(rsp3);

	self.buffers.chan1 = rsp;
	self.buffers.chan2 = rsp2;
	self.buffers.chan3 = rsp3;
}

void ana_adc_read(double *rsp, uint8_t channel)
{
	channel -= ADC_BASE_SUBTRACTIV; /* Adjust channel number to match ADC input */

	adc_select_input(channel);

	raw_data = adc_read();

	result = (double)raw_data * ADC_FACTOR_CONVERSION;
	result *= 1000.0; /* Convert to millivolts */

	*rsp = result;
}

void ana_adc_read_all(uint32_t samples)
{
	samples = (samples <= 0) ? 1 : samples;

	adc_run(true);

	for (uint32_t i = 0; i < samples; i++) {
		ana_adc_read(self.buffers.chan1 + i, PICO_DEFAULT_ADC_CHANNEL_1);
		ana_adc_read(self.buffers.chan2 + i, PICO_DEFAULT_ADC_CHANNEL_2);
		ana_adc_read(self.buffers.chan3 + i, PICO_DEFAULT_ADC_CHANNEL_3);
	}

	adc_run(false);
}

struct buffs *ana_adc_get_buffs()
{
	return &self.buffers;
}

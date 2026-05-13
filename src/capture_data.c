/** -------------------------------------------------------------
 * @file capture_data.c
 * @brief Capture data module implementation
 *
 * @author    Vinicius Rafael Marques de Carvalho <vinicius.carvalho@edge.ufal.br>
 * @author   João Matheus Nascimento Dias <joao.dias@edge.ufal.br>
 * @version   0.1
 * @date      28/01/2026
 * @copyright  Copyright (c) 2026
 *  ------------------------------------------------------------*/

#include "capture_data.h"
#include "module.h"
#include "handles/sigrok_handler.h"
#include <stdint.h>

#include <hardware/adc.h>

#define ADC_MAX_RATE         500000
#define DIGITAL_CHANNEL_SIZE 12
#define ANALOG_CHANNEL_SIZE  3

static struct pulseview_sample_config *cfg;

int ana_capture_init(struct ana_module_system *config)
{
	if (ana_module_pio_dma_is_busy(config)) {
		ana_module_pio_dma_abort(config);
	}

	cfg = ana_sigrok_get_sample_config();

	memset(config->dma.dma_buffer, 0, cfg->samples * sizeof(uint16_t));

	return PICO_OK;
}

int ana_capture_data_get_analog_channels_count(uint8_t analog_mask)
{
	int enabled_analog_channel_count = 0;
	for (int i = 0; i < ANALOG_CHANNEL_SIZE; i++) {
		if (analog_mask & (1 << i)) {
			enabled_analog_channel_count++;
		}
	}
	return enabled_analog_channel_count;
}

static int ana_get_first_analog_channel(uint8_t analog_mask)
{
	for (int i = 0; i < ANALOG_CHANNEL_SIZE; i++) {
		if (analog_mask & (1 << i)) {
			return i;
		}
	}
	return 0;
}

void ana_capture_data_start(struct ana_module_system *config)
{
	ana_capture_init(config);

	ana_module_pio_dma_start(config);
	ana_module_pio_dma_wait(config);
}
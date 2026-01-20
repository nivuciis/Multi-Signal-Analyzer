/** -------------------------------------------------------------
 * @file capture_data.c
 * @brief Capture data module implementation
 *
 * @author    Vinicius Rafael Marques de Carvalho <vinicius.carvalho@edge.ufal.br>
 * @version   0.1
 * @date      15/01/2026
 * @copyright  Copyright (c) 2026
 *  ------------------------------------------------------------*/

#include "capture.pio.h"
#include "capture_data.h"

#include <stdio.h>

#include <hardware/adc.h>
#include <hardware/clocks.h>
#include <hardware/dma.h>
#include <hardware/pio.h>
#include <tusb.h>

static PIO pio = pio1;
static uint sm;
static uint pio_offset;
static int dma_digital_channel;
static int dma_analog_channel;

uint32_t digital_capture_buffer[CAPTURE_DEPTH];
uint8_t analog_capture_buffer[CAPTURE_DEPTH * 3];

int ana_capture_init()
{
	if (!pio_can_add_program(pio, &capture_prog_program)) {
		return PICO_ERROR_GENERIC;
	}
	pio_offset = pio_add_program(pio, &capture_prog_program);

	sm = pio_claim_unused_sm(pio, true);
	dma_digital_channel = dma_claim_unused_channel(true);
	dma_analog_channel = dma_claim_unused_channel(true);

	if (sm == -1 || dma_digital_channel == -1 || dma_analog_channel == -1) {
		return PICO_ERROR_GENERIC;
	}

	pio_sm_config sm_config = capture_prog_program_get_default_config(pio_offset);
	uint base_pin = 2;
	sm_config_set_in_pins(&sm_config, base_pin);

	for (int i = 0; i < 16; i++) {
		pio_gpio_init(pio, base_pin + i);
	}
	pio_sm_set_consecutive_pindirs(pio, sm, base_pin, 16, false);
	sm_config_set_in_shift(&sm_config, false, true, 32);
	sm_config_set_fifo_join(&sm_config, PIO_FIFO_JOIN_RX);

	int err = pio_sm_init(pio, sm, pio_offset, &sm_config);
	if (err != PICO_OK) {
		return err;
	}
	adc_init();
	adc_gpio_init(26);
	adc_gpio_init(27);
	adc_gpio_init(28);

	return PICO_OK;
}

/**
 * @brief Configure the DMA channel to capture analog data from the ADC
 *
 * @param samples_to_capture describes the number of samples to capture
 * @param sample_rate_hz  describes the sample rate in Hz
 */
static void ana_setup_analog_channel(uint32_t samples_to_capture, uint32_t sample_rate_hz)
{
	adc_select_input(0);
	adc_set_round_robin(0xEF);
	adc_fifo_setup(true, true, 1, false, true);

	adc_set_clkdiv(48000000.f / (sample_rate_hz * 3));

	dma_channel_abort(dma_analog_channel);
	dma_channel_config ana_cfg = dma_channel_get_default_config(dma_analog_channel);
	channel_config_set_transfer_data_size(&ana_cfg, DMA_SIZE_8);
	channel_config_set_read_increment(&ana_cfg, false);
	channel_config_set_write_increment(&ana_cfg, true);
	channel_config_set_dreq(&ana_cfg, DREQ_ADC);

	dma_channel_configure(dma_analog_channel, &ana_cfg, analog_capture_buffer, &adc_hw->fifo,
			      samples_to_capture * 3, false);
}

/**
 * @brief Configure the DMA channel and PIO program to capture digital data
 *
 * @param samples_to_capture describes the number of samples to capture
 * @param sample_rate_hz describes the sample rate in Hz
 */
static void ana_setup_digital_channel(uint32_t samples_to_capture, uint32_t sample_rate_hz)
{
	pio_sm_set_enabled(pio, sm, false);
	dma_channel_abort(dma_digital_channel);

	dma_channel_config dma_cfg = dma_channel_get_default_config(dma_digital_channel);
	channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_32);

	channel_config_set_read_increment(&dma_cfg, false);
	channel_config_set_write_increment(&dma_cfg, true);

	channel_config_set_dreq(&dma_cfg, pio_get_dreq(pio, sm, false));

	dma_channel_configure(dma_digital_channel, &dma_cfg, digital_capture_buffer, &pio->rxf[sm],
			      samples_to_capture, false);
}

void ana_capture_data(uint32_t sample_count, uint32_t sample_rate_hz, uint32_t *capture_buffer)
{
	if (sample_count > CAPTURE_DEPTH) {
		sample_count = CAPTURE_DEPTH;
	}
	ana_setup_digital_channel(sample_count, sample_rate_hz);
	ana_setup_analog_channel(sample_count, sample_rate_hz);

	adc_run(true);
	pio_sm_set_enabled(pio, sm, true);

	dma_start_channel_mask((1u << dma_digital_channel) | (1u << dma_analog_channel));

	while (dma_channel_is_busy(dma_digital_channel) ||
	       dma_channel_is_busy(dma_analog_channel)) {
		tud_task();
	}

	adc_run(false);
	pio_sm_set_enabled(pio, sm, false);
	adc_fifo_drain();
}

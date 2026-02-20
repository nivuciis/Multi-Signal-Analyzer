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
static const uint32_t adc_max_total_rate = 500000;

static uint16_t digital_capture_buffer[CAPTURE_DEPTH];
static uint8_t analog_capture_buffer[CAPTURE_DEPTH * 3];

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
	sm_config_set_in_pins(&sm_config, BASE_PIN);

	for (int i = 0; i < 16; i++) {
		pio_gpio_init(pio, BASE_PIN + i);
		gpio_set_dir(BASE_PIN + i, GPIO_IN);
		gpio_set_pulls(BASE_PIN + i, true, false);
	}

	pio_sm_set_consecutive_pindirs(pio, sm, BASE_PIN, 16, false);
	sm_config_set_in_shift(&sm_config, false, true, 16);
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

int ana_get_analog_channels_count(uint8_t analog_mask)
{
	int enabled_analog_channel_count = 0;
	for (int i = 0; i < 3; i++) {
		if (analog_mask & (1 << i)) {
			enabled_analog_channel_count++;
		}
	}
	return enabled_analog_channel_count;
}

/**
 * @brief Get the first analog channel enabled
 *
 * @param analog_mask
 * @return int contains the first analog channel enabled
 */
static int ana_get_first_analog_channel(uint8_t analog_mask)
{
	for (int i = 0; i < 3; i++) {
		if (analog_mask & (1 << i)) {
			return i;
		}
	}
	return 0;
}

/**
 * @brief Configure the DMA channel to capture analog data from the ADC
 *
 * @param samples_to_capture describes the number of samples to capture
 * @param sample_rate_hz  describes the sample rate in Hz
 * @param analog_mask Bitmask to indicate which analog channels to capture
 *
 * @note The sample rate is divided among the enabled analog channels and has a maximum of 48 MHz
 */
static void ana_setup_analog_channel(uint32_t samples_to_capture, uint32_t sample_rate_hz,
				     uint8_t analog_mask)
{
	if (sample_rate_hz >= 48000000) {
		sample_rate_hz = 48000000;
	}
	int analog_channels_count = ana_get_analog_channels_count(analog_mask);
	if (analog_channels_count == 0) {
		return;
	}
	adc_select_input(ana_get_first_analog_channel(analog_mask));
	adc_set_round_robin(analog_mask);

	adc_fifo_setup(true, true, 1, false, true);

	uint32_t max_rate_per_channel = adc_max_total_rate / analog_channels_count;
	if (sample_rate_hz > max_rate_per_channel) {
		sample_rate_hz = max_rate_per_channel;
	}
	adc_set_clkdiv(48000000.f / (sample_rate_hz * analog_channels_count));

	dma_channel_abort(dma_analog_channel);
	dma_channel_config analog_dma_cfg = dma_channel_get_default_config(dma_analog_channel);

	channel_config_set_transfer_data_size(&analog_dma_cfg, DMA_SIZE_8);
	channel_config_set_read_increment(&analog_dma_cfg, false);
	channel_config_set_write_increment(&analog_dma_cfg, true);
	channel_config_set_dreq(&analog_dma_cfg, DREQ_ADC);

	dma_channel_configure(dma_analog_channel, &analog_dma_cfg, analog_capture_buffer,
			      &adc_hw->fifo, samples_to_capture * analog_channels_count, false);
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

	uint32_t system_clk = clock_get_hz(clk_sys);
	float clkdiv = (float)system_clk / (float)sample_rate_hz;
	if (clkdiv < 1.0f) {
		clkdiv = 1.0f;
	}
	pio_sm_set_clkdiv(pio, sm, clkdiv);

	dma_channel_config digital_dma_cfg = dma_channel_get_default_config(dma_digital_channel);
	channel_config_set_transfer_data_size(&digital_dma_cfg, DMA_SIZE_16);

	channel_config_set_read_increment(&digital_dma_cfg, false);
	channel_config_set_write_increment(&digital_dma_cfg, true);
	channel_config_set_dreq(&digital_dma_cfg, pio_get_dreq(pio, sm, false));

	dma_channel_configure(dma_digital_channel, &digital_dma_cfg, digital_capture_buffer,
			      &pio->rxf[sm], samples_to_capture, false);
}

void ana_capture_data(uint32_t sample_count, uint32_t sample_rate_hz, uint8_t analog_mask)
{
	if (sample_count > CAPTURE_DEPTH) {
		sample_count = CAPTURE_DEPTH;
	}

	ana_setup_digital_channel(sample_count, sample_rate_hz);
	bool has_analog = analog_mask;

	if (has_analog != 0) {
		ana_setup_analog_channel(sample_count, sample_rate_hz, analog_mask);
	}

	int dma_mask = (1u << dma_digital_channel);
	if (has_analog != 0) {
		dma_mask |= (1u << dma_analog_channel);
	}

	dma_start_channel_mask(dma_mask);

	if (has_analog != 0) {
		adc_run(true);
	}
	pio_sm_set_enabled(pio, sm, true);

	while (dma_channel_is_busy(dma_digital_channel) ||
	       (has_analog && dma_channel_is_busy(dma_analog_channel))) {
		tud_task();
	}

	adc_run(false);
	pio_sm_set_enabled(pio, sm, false);
	adc_fifo_drain();
	adc_set_round_robin(0);
}

uint16_t *ana_get_digital_capture_buffer(void)
{
	return digital_capture_buffer;
}

uint8_t *ana_get_analog_capture_buffer(void)
{
	return analog_capture_buffer;
}

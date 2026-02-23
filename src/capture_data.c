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
#include "log.h"

#include <stdio.h>

#include <hardware/adc.h>
#include <hardware/clocks.h>
#include <hardware/dma.h>
#include <hardware/pio.h>
#include <tusb.h>

#define ADC_MAX_RATE         500000
#define DIGITAL_CHANNEL_SIZE 16
#define ANALOG_CHANNEL_SIZE  3
#define LOG_MODULE           "capture_data"

/*
 * TODO: when the modules were full implemented, switch to a module struct to encapsulate the
 * capture data state @JoaoMatheusND
 */
static struct CAPTURE_DATA {
	PIO pio;
	uint sm;
	uint pio_offset;
	int dma_digital_channel;
	int dma_analog_channel;
	uint16_t digital_capture_buffer[CAPTURE_BUFFER_SIZE];
	uint8_t analog_capture_buffer[CAPTURE_BUFFER_SIZE * 3];
} self = {
	.pio = pio1,
	.sm = 0,
	.pio_offset = 0,
	.dma_digital_channel = 0,
	.dma_analog_channel = 0,
	.digital_capture_buffer = {0},
	.analog_capture_buffer = {0},
};

int ana_capture_init()
{
	if (!pio_can_add_program(self.pio, &capture_prog_program)) {
		log_debug(LOG_MODULE, "Failed to add program to PIO");
		return PICO_ERROR_NOT_PERMITTED;
	}

	self.pio_offset = pio_add_program(self.pio, &capture_prog_program);

	self.sm = pio_claim_unused_sm(self.pio, true);
	self.dma_digital_channel = dma_claim_unused_channel(true);
	self.dma_analog_channel = dma_claim_unused_channel(true);

	if (self.sm == -1 || self.dma_digital_channel == -1 || self.dma_analog_channel == -1) {
		log_debug(LOG_MODULE, "State machine not available or channels args are invalid");
		return PICO_ERROR_INVALID_ARG;
	}

	pio_sm_config sm_config = capture_prog_program_get_default_config(self.pio_offset);
	sm_config_set_in_pins(&sm_config, BASE_PIN);

	for (int i = 0; i < DIGITAL_CHANNEL_SIZE; i++) {
		pio_gpio_init(self.pio, BASE_PIN + i);
		gpio_set_dir(BASE_PIN + i, GPIO_IN);
		gpio_set_pulls(BASE_PIN + i, true, false);
	}

	pio_sm_set_consecutive_pindirs(self.pio, self.sm, BASE_PIN, 16, false);
	sm_config_set_in_shift(&sm_config, false, true, 16);
	sm_config_set_fifo_join(&sm_config, PIO_FIFO_JOIN_RX);

	int err = pio_sm_init(self.pio, self.sm, self.pio_offset, &sm_config);
	if (err != PICO_OK) {
		return err;
	}

	adc_init();
	adc_gpio_init(26);
	adc_gpio_init(27);
	adc_gpio_init(28);

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

/**
 * @brief Get the first analog channel enabled
 *
 * @param analog_mask
 * @return int contains the first analog channel enabled
 */
static int ana_get_first_analog_channel(uint8_t analog_mask)
{
	for (int i = 0; i < ANALOG_CHANNEL_SIZE; i++) {
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
	int analog_channels_count = ana_capture_data_get_analog_channels_count(analog_mask);
	if (analog_channels_count == 0) {
		return;
	}
	adc_select_input(ana_get_first_analog_channel(analog_mask));
	adc_set_round_robin(analog_mask);

	adc_fifo_setup(true, true, 1, false, true);

	uint32_t max_rate_per_channel = ADC_MAX_RATE / analog_channels_count;
	if (sample_rate_hz > max_rate_per_channel) {
		sample_rate_hz = max_rate_per_channel;
	}
	adc_set_clkdiv(48000000.f / (sample_rate_hz * analog_channels_count));

	dma_channel_abort(self.dma_analog_channel);
	dma_channel_config analog_dma_cfg = dma_channel_get_default_config(self.dma_analog_channel);

	channel_config_set_transfer_data_size(&analog_dma_cfg, DMA_SIZE_8);
	channel_config_set_read_increment(&analog_dma_cfg, false);
	channel_config_set_write_increment(&analog_dma_cfg, true);
	channel_config_set_dreq(&analog_dma_cfg, DREQ_ADC);

	dma_channel_configure(self.dma_analog_channel, &analog_dma_cfg, self.analog_capture_buffer,
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
	pio_sm_set_enabled(self.pio, self.sm, false);
	dma_channel_abort(self.dma_digital_channel);

	uint32_t system_clk = clock_get_hz(clk_sys);
	float clkdiv = (float)system_clk / (float)sample_rate_hz;
	if (clkdiv < 1.0f) {
		clkdiv = 1.0f;
	}
	pio_sm_set_clkdiv(self.pio, self.sm, clkdiv);

	dma_channel_config digital_dma_cfg =
		dma_channel_get_default_config(self.dma_digital_channel);
	channel_config_set_transfer_data_size(&digital_dma_cfg, DMA_SIZE_16);

	channel_config_set_read_increment(&digital_dma_cfg, false);
	channel_config_set_write_increment(&digital_dma_cfg, true);
	channel_config_set_dreq(&digital_dma_cfg, pio_get_dreq(self.pio, self.sm, false));

	dma_channel_configure(self.dma_digital_channel, &digital_dma_cfg,
			      self.digital_capture_buffer, &self.pio->rxf[self.sm],
			      samples_to_capture, false);
}

void ana_capture_data_start(uint32_t sample_count, uint32_t sample_rate_hz, uint8_t analog_mask)
{
	if (sample_count > CAPTURE_BUFFER_SIZE) {
		sample_count = CAPTURE_BUFFER_SIZE;
	}

	ana_setup_digital_channel(sample_count, sample_rate_hz);

	int dma_mask = (1u << self.dma_digital_channel);
	if (analog_mask != 0) {
		ana_setup_analog_channel(sample_count, sample_rate_hz, analog_mask);
		dma_mask |= (1u << self.dma_analog_channel);
	}

	dma_start_channel_mask(dma_mask);

	if (analog_mask != 0) {
		adc_run(true);
	}
	pio_sm_set_enabled(self.pio, self.sm, true);

	while (dma_channel_is_busy(self.dma_digital_channel) ||
	       ((analog_mask != 0) && dma_channel_is_busy(self.dma_analog_channel))) {
		tud_task();
	}

	adc_run(false);
	pio_sm_set_enabled(self.pio, self.sm, false);
	adc_fifo_drain();
	adc_set_round_robin(0);
}

uint16_t *ana_capture_data_get_digital_capture_buffer(void)
{
	return self.digital_capture_buffer;
}

uint8_t *ana_capture_data_get_analog_capture_buffer(void)
{
	return self.analog_capture_buffer;
}

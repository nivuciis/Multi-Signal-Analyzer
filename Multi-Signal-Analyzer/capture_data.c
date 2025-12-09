/** -------------------------------------------------------------
 * @file capture_data.c
 * @brief Capture data module implementation
 *
 * @author    Vinicius Rafael Marques de Carvalho <vinicius.carvalho@edge.ufal.br>
 * @version   v1.0
 * @date      05/12/2025
 * @copyright
 *  ------------------------------------------------------------*/

#include "capture.pio.h"
#include "capture_data.h"

#include <stdio.h>

#include <hardware/clocks.h>
#include <hardware/dma.h>
#include <hardware/pio.h>
#include <tusb.h>

static PIO pio = pio1;
static uint sm;
static uint pio_offset;
static int dma_chan;

int capture_init()
{
	if (!pio_can_add_program(pio, &capture_prog_program)) {
		return PICO_ERROR_GENERIC;
	}
	pio_offset = pio_add_program(pio, &capture_prog_program);

	sm = pio_claim_unused_sm(pio, true);
	dma_chan = dma_claim_unused_channel(true);
	if (sm == -1 || dma_chan == -1) {
		return PICO_ERROR_GENERIC;
	}

	int err = pio_sm_init(pio, sm, pio_offset, NULL);
	if (err != PICO_OK) {
		return err;
	}

	return PICO_OK;
}

void send_captured_data(const uint32_t *buffer, uint32_t num_samples)
{
	tud_cdc_write((const uint8_t *)buffer, num_samples * sizeof(uint32_t));
	tud_cdc_write_flush();
}

/**
 * @brief Set up all the configurations to start the PIO state machines and DMA based on the
 * parameters passed
 *
 * @param samples_to_capture
 * @param sample_rate_hz
 */
static void setup_capture(uint32_t samples_to_capture, uint32_t sample_rate_hz, uint32_t *capture_buffer)
{

	pio_sm_set_enabled(pio, sm, false);
	dma_channel_abort(dma_chan);

	pio_sm_config config_sm = pio_get_default_sm_config();

	sm_config_set_in_pins(&config_sm, 0);

	for (int i = 0; i < 12; i++) {
		pio_gpio_init(pio, i);
	}

	pio_sm_set_consecutive_pindirs(pio, sm, 0, 12, false);

	sm_config_set_wrap(&config_sm, pio_offset, pio_offset + capture_prog_program.length - 1);

	float div = (float)clock_get_hz(clk_sys) / (float)(sample_rate_hz);
	sm_config_set_clkdiv(&config_sm, div);

	sm_config_set_in_shift(&config_sm, false, true, 32);
	sm_config_set_fifo_join(&config_sm, PIO_FIFO_JOIN_RX);

	pio_sm_init(pio, sm, pio_offset, &config_sm);
	pio_sm_clear_fifos(pio, sm);

	dma_channel_config dma_cfg = dma_channel_get_default_config(dma_chan);
	channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_32);

	channel_config_set_read_increment(&dma_cfg, false);
	channel_config_set_write_increment(&dma_cfg, true);

	channel_config_set_dreq(&dma_cfg, pio_get_dreq(pio, sm, false));

	dma_channel_configure(dma_chan, &dma_cfg, capture_buffer, &pio->rxf[sm], samples_to_capture,
			      false);
}

void capture_data(uint32_t sample_count, uint32_t sample_rate_hz, uint32_t *capture_buffer)
{
	uint32_t samples_to_capture = sample_count;
	if (samples_to_capture > CAPTURE_BUFFER_SIZE) {
		samples_to_capture = CAPTURE_BUFFER_SIZE;
	}
	setup_capture(samples_to_capture, sample_rate_hz, capture_buffer);

	pio_sm_set_enabled(pio, sm, true);
	dma_channel_start(dma_chan);

	dma_channel_wait_for_finish_blocking(dma_chan);
	pio_sm_set_enabled(pio, sm, false);
}

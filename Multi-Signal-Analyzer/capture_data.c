/** -------------------------------------------------------------
 * @file capture_data.c
 * @brief Capture data module implementation
 *
 * @author    Vinicius Rafael Marques de Carvalho vinicius.carvalho@edge.ufal.br
 * @version   v1.0
 * @date      28/11/2025
 * @copyright
 *  ------------------------------------------------------------*/

#include "capture.pio.h"
#include "capture_data.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "tusb.h"

#include <stdio.h>

static PIO pio = pio1;
static uint sm;
static uint pio_offset;
static int dma_chan;

// Buffer to hold captured data
#define CAPTURE_BUFFER_SIZE (16 * 1024)
static uint32_t capture_buffer[CAPTURE_BUFFER_SIZE];

int capture_init()
{
    if (!pio_can_add_program(pio, &capture_prog_program)) {
        return PICO_ERROR_GENERIC;
    }
	pio_offset = pio_add_program(pio, &capture_prog_program);
	sm = pio_claim_unused_sm(pio, true);
	dma_chan = dma_claim_unused_channel(true);
    if(sm == -1 || dma_chan == -1){
        return PICO_ERROR_GENERIC;
    }
    int err = pio_sm_init(pio, sm, pio_offset, NULL);
	if( err != PICO_OK  ){
        return err;
    }
    return PICO_OK;
}

/**
* @brief Send captured data through USB CDC
* @param buffer Pointer to the buffer containing captured data
* @param num_samples Number of samples to send
*/
static inline int send_captured_data(const uint32_t *buffer, uint32_t num_samples)
{

	tud_cdc_write((const uint8_t *)buffer, num_samples * sizeof(uint32_t));
	tud_cdc_write_flush();
	return PICO_OK;
}

/**
 * @brief Arm the PIO and DMA for capturing data
 * @return PICO_OK on success, error code otherwise
 */

static void arm_capture(uint32_t samples_to_capture, uint32_t sample_rate){
    // setup PIO and DMA for capture
	pio_sm_set_enabled(pio, sm, false);
	dma_channel_abort(dma_chan);

	pio_sm_config config_sm = pio_get_default_sm_config();

	// sm pin base definition
	sm_config_set_in_pins(&config_sm, 0);

	// initialize GPIOs
	for (int i = 0; i < 12; i++) {
		pio_gpio_init(pio, i);
	}

	// Define pins as inputs
	pio_sm_set_consecutive_pindirs(pio, sm, 0, 12, false);

	// configure the state machine
	sm_config_set_wrap(&config_sm, pio_offset, pio_offset + capture_prog_program.length - 1);

	float div = (float)clock_get_hz(clk_sys) / (float)(sample_rate);
	sm_config_set_clkdiv(&config_sm, div);

	// auto-push when FIFO is full (32 bits)
	sm_config_set_in_shift(&config_sm, false, true, 32);
	sm_config_set_fifo_join(&config_sm, PIO_FIFO_JOIN_RX);

	pio_sm_init(pio, sm, pio_offset, &config_sm);
	pio_sm_clear_fifos(pio, sm);

	// DMA configuration
	dma_channel_config dma_cfg = dma_channel_get_default_config(dma_chan);
	channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_32);

	// read from PIO RX FIFO, write to RAM buffer
	channel_config_set_read_increment(&dma_cfg, false);
	channel_config_set_write_increment(&dma_cfg, true);

	channel_config_set_dreq(&dma_cfg, pio_get_dreq(pio, sm, false));

	dma_channel_configure(dma_chan, &dma_cfg,
			      capture_buffer, // destination
			      &pio->rxf[sm],  // source
			      samples_to_capture,
			      false // do not start yet
	);
    return;
}


/**
 * @brief Start capturing process
 *  @param sample_count Number of samples to capture
 */
void capture_data(uint32_t sample_count, uint32_t sample_rate)
{
    uint32_t samples_to_capture = sample_count;
	if (samples_to_capture > CAPTURE_BUFFER_SIZE) {
		samples_to_capture = CAPTURE_BUFFER_SIZE;
	}
	arm_capture(samples_to_capture, sample_rate);

	// Activate PIO state machine and start DMA transfer
	pio_sm_set_enabled(pio, sm, true);
    dma_channel_start(dma_chan);

	dma_channel_wait_for_finish_blocking(dma_chan);

	// Stop PIO state machine and send data
	pio_sm_set_enabled(pio, sm, false);
	int err = send_captured_data(capture_buffer, samples_to_capture);
	if (err != PICO_OK) {
		tud_cdc_write_str("ERROR: Data send failed\r\n");
		tud_cdc_write_flush();
	}
    return ;
}

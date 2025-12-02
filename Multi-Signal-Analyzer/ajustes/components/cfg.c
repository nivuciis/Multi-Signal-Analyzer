#include "../includes/cfg.h"
#include "../includes/wdg.h"

#include <pico/types.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "capture.pio.h" 
#include "includes/util.h"

#define CAPTURE_BUFFER_SIZE (8 * 1) // 8 samples
static uint32_t capture_buffer[CAPTURE_BUFFER_SIZE];

// TODO: Add error handling
// TODO: tornar tudo generico no futuro
// TODO: Switch the commum params as pio, sm and dma_channel to a aka struct self at future

//PIO and State machine used - TODO: CENTRALIZAR NA MAIN
static PIO pio = pio1;
static uint sm;
static uint pio_offset;

// DMA channel
static int dma_chan;

void cfg_init() {
    pio_offset = pio_add_program(pio, &capture_prog_program);
    sm = pio_claim_unused_sm(pio, true);
    dma_chan = dma_claim_unused_channel(true);
    pio_sm_init(pio, sm, pio_offset, NULL);
    feed_watchdog();
}

uint32_t capture_init(uint32_t sample_count, uint32_t sample_rate) {
    cfg_initial_abort(pio, sm, dma_chan);

    pio_sm_config c = cfg_pio(capture_prog_program_get_default_config, pio_offset, 0, capture_prog_program, sample_rate);
    pio_sm_init(pio, sm, pio_offset, &c);

    uint32_t samples_to_capture = sample_count;
    if (samples_to_capture > CAPTURE_BUFFER_SIZE) {
        samples_to_capture = CAPTURE_BUFFER_SIZE;
    }
    cfg_dma(pio, sm, dma_chan, samples_to_capture, capture_buffer);

    return samples_to_capture;
}


void cfg_initial_abort(PIO pio, uint sm, uint dma_chan) {
    /*>! Stops any PIO or DMA running */
    dma_channel_abort(dma_chan);
    pio_sm_set_enabled(pio, sm, false);
    pio_sm_clear_fifos(pio, sm);
}

pio_sm_config cfg_pio(get_default_config get_cfg, uint offset, uint pin_base, struct pio_program prog, uint32_t sample_rate) {

    /*>! Get global offset from pio init */
    pio_sm_config c = get_cfg(offset);

    /*>! Set pin directions and set pin_base */
    sm_config_set_in_pins(&c, pin_base);

    /*>! Define the .Wrap from PIO asm */
    sm_config_set_wrap(&c, offset, offset + prog.length - 1);

    /*>! Sets PIO clock (2 cycles for sample) */
    float div = (float) clock_get_hz(clk_sys) / (float) (sample_rate * 1);
    sm_config_set_clkdiv(&c, div);

    /*>! Enable AUTOPUSH opush blockn FIFO
     * false = shift right, true = autopush, 32 = threshold
     * Since mov always carry 32 bits the threshold should not change
     */
    sm_config_set_in_shift(&c, false, true, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    return c;
}

void cfg_dma(PIO pio, uint sm, uint dma_chan, uint32_t sample_count, uint32_t *buffer) {

    assert(buffer != NULL);

    /*>! Get default DMA config */
    dma_channel_config dma_cfg = dma_channel_get_default_config(dma_chan);

    /*>! Modify the default configure the DMA channel */
    channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_32); /**> 32 bits transfer */
    channel_config_set_read_increment(&dma_cfg, false);
    channel_config_set_write_increment(&dma_cfg, true);
    channel_config_set_dreq(&dma_cfg, pio_get_dreq(pio, sm, false));

    dma_channel_configure(dma_chan, &dma_cfg,
                          buffer,        // Destination pointer
                          &pio->rxf[sm], // Source pointer
                          sample_count,  // Number of transfers
                          true           // do NOT start yet; caller should start explicitly
    );
}

void cfg_start_capture() {
    pio_sm_set_enabled(pio, sm, true);
    
    dma_channel_wait_for_finish_blocking(dma_chan);

    pio_sm_set_enabled(pio, sm, false);

    print_buffer(capture_buffer);
}
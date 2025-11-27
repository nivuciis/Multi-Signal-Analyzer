#include "capture_data.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "capture.pio.h" 
#include <stdio.h>   
#include "tusb.h"    

static PIO pio = pio1;
static uint sm;
static uint pio_offset;
static int dma_chan;

// Buffer to hold captured data
#define CAPTURE_BUFFER_SIZE (16 * 1024)
static uint32_t capture_buffer[CAPTURE_BUFFER_SIZE];

void capture_init() {
    pio_offset = pio_add_program(pio, &capture_prog_program);
    sm = pio_claim_unused_sm(pio, true);
    dma_chan = dma_claim_unused_channel(true);
    pio_sm_init(pio, sm, pio_offset, NULL);
}

//Send captured data over USB CDC
static void send_capture_data(const uint32_t *buffer, uint32_t num_samples) {
    
    tud_cdc_write((const uint8_t*)buffer, num_samples * sizeof(uint32_t));
    tud_cdc_write_flush();
}

void capture_arm_and_send(uint32_t sample_count, uint32_t sample_rate){
    // setup PIO and DMA for capture
    pio_sm_set_enabled(pio, sm, false);
    dma_channel_abort(dma_chan);

    pio_sm_config c = pio_get_default_sm_config();

    //sm pin base definition
    sm_config_set_in_pins(&c, 0); 
    
    // initialize GPIOs
    for(int i = 0; i < 12; i++) {
        pio_gpio_init(pio, i);
    }
    
    // Define pins as inputs
    pio_sm_set_consecutive_pindirs(pio, sm, 0, 12, false);

    // configure the state machine
    sm_config_set_wrap(&c, pio_offset, pio_offset + capture_prog_program.length - 1);
    
    float div = (float)clock_get_hz(clk_sys) / (float)(sample_rate);
    sm_config_set_clkdiv(&c, div);

    // auto-push when FIFO is full (32 bits)
    sm_config_set_in_shift(&c, false, true, 32); 
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);

    pio_sm_init(pio, sm, pio_offset, &c); 
    pio_sm_clear_fifos(pio, sm);

    // DMA configuration
    dma_channel_config dma_cfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_32);
    
    // read from PIO RX FIFO, write to RAM buffer
    channel_config_set_read_increment(&dma_cfg, false); 
    channel_config_set_write_increment(&dma_cfg, true);
    
    channel_config_set_dreq(&dma_cfg, pio_get_dreq(pio, sm, false));

    uint32_t samples_to_capture = sample_count;
    if (samples_to_capture > CAPTURE_BUFFER_SIZE) {
        samples_to_capture = CAPTURE_BUFFER_SIZE;
    }

    dma_channel_configure(
        dma_chan,
        &dma_cfg,
        capture_buffer,      // destination
        &pio->rxf[sm],       // source
        samples_to_capture,
        true                 // start immediately
    );

    // start PIO
    pio_sm_set_enabled(pio, sm, true); 
    
    // block until DMA transfer is done
    dma_channel_wait_for_finish_blocking(dma_chan);

    // finish capture and send data trhough USB
    pio_sm_set_enabled(pio, sm, false);
    send_capture_data(capture_buffer, samples_to_capture);
}
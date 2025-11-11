#include "capture_data.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "capture.pio.h" 
#include <stdio.h>       

//PIO and State machine used
static PIO pio = pio1;
static uint sm ;
static uint pio_offset;
// DMA channel
static int dma_chan;

// 512k samples * 4 bytes/sample = 2.097.152 bytes (2MB)
#define CAPTURE_BUFFER_SIZE ( 8 * 1024)
static uint32_t capture_buffer[CAPTURE_BUFFER_SIZE];

void capture_init() {
    pio_offset = pio_add_program(pio, &capture_prog_program);

    // Claim an unused state machine (State machine is used in led control too so its necessary to see that...) 
    sm = pio_claim_unused_sm(pio, true);

    // Claim an Unused DMA channel
    dma_chan = dma_claim_unused_channel(true);

    pio_sm_init(pio, sm, pio_offset, NULL);
}

/**
 * @brief Send the binary buffer via USB 
 * @param buffer pointer and number of samples designated by the host
 * 
 */
static void send_capture_data(const uint32_t *buffer, uint32_t num_samples) {
    fwrite(buffer, sizeof(uint32_t), num_samples, stdout);
    fflush(stdout);
}

/**
 * @brief Start the PIO program to capture data based on the mov instruction 
 * Set the DMA channel to transfer the data from RXFIFO directly to the buffer
 */
void capture_arm_and_send(uint32_t sample_count, uint32_t sample_rate){
    //Stops any PIO or DMA running 
    pio_sm_set_enabled(pio, sm, false);
    dma_channel_abort(dma_chan);

    // Get global offset from pio init
    pio_sm_config c = pio_get_default_sm_config();

    //Set all 13 pins as inputs ( Starting on GPIO 8)
    
    sm_config_set_in_pins(&c, 8); 
    
    // Define the .Wrap from PIO asm
    sm_config_set_wrap(&c, pio_offset, pio_offset + capture_prog_program.length - 1);
    
    // Sets PIO clock (2 cycles for sample)
    float div = (float)clock_get_hz(clk_sys) / (float)(sample_rate * 1);
    sm_config_set_clkdiv(&c, div);

    // Enable AUTOPUSH on FIFO
    // false = shift right, true = autopush, 32 = threshold
    //Since mov always carry 32 bits the threshold should not change
    sm_config_set_in_shift(&c, false, true, 32); 
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);

    pio_sm_init(pio, sm, pio_offset, &c); 
    pio_sm_clear_fifos(pio, sm);

    // DMA config
    dma_channel_config dma_cfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&dma_cfg, true);
    channel_config_set_write_increment(&dma_cfg, false);
    channel_config_set_dreq(&dma_cfg, pio_get_dreq(pio, sm, false));

    // prevent OVERFLOW because if the host ask for a sample count greater then buffer size it becames buffer size
    uint32_t samples_to_capture = sample_count;
    if (samples_to_capture > CAPTURE_BUFFER_SIZE) {
        samples_to_capture = CAPTURE_BUFFER_SIZE;
    }

    // Start DMA
    dma_channel_configure(
        dma_chan,
        &dma_cfg,
        capture_buffer,         // Destination
        &pio->rxf[sm],          // Source
        samples_to_capture,
        true                    // Start the transfer immediately
    );

    //Start the PIO and wait
    pio_sm_set_enabled(pio, sm, true); 
    printf("TEST PRINT LINE 98\n");
    dma_channel_wait_for_finish_blocking(dma_chan);

    // Finish capture and send data
    pio_sm_set_enabled(pio, sm, false);
    printf("Buffer sended\n");
    send_capture_data(capture_buffer, samples_to_capture);
}

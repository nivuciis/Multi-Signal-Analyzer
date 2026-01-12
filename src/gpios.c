/*******************************************************************
 * @file gpios.c
 *
 * @brief GPIOs module test
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 09/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/

#include "bring_up_gpios.pio.h"
#include "gpios.h"

#include <assert.h>

#include <hardware/dma.h>
#include <hardware/pio.h>

static void gpios_cfg_dma(uint32_t *dma_buffer)
{
	dma_channel_config dma_cfg = dma_channel_get_default_config(dma_gpios_chan);

	channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_16);
	channel_config_set_read_increment(&dma_cfg, false);
	channel_config_set_write_increment(&dma_cfg, true);
	channel_config_set_dreq(&dma_cfg, pio_get_dreq(pio_gpios, sm_gpios, false));

	dma_channel_configure(dma_gpios_chan, &dma_cfg, dma_buffer, &pio_gpios->rxf[sm_gpios],
			      GPIOS_NUM_PINS, false);
}

void ana_gpios_init(uint16_t *dma_buffer, double clk_sys)
{
	assert(dma_buffer != NULL);

	pio_gpios_offset = pio_add_program(pio_gpios, &bring_up_gpios_program);
	sm_gpios = pio_claim_unused_sm(pio_gpios, true);
	dma_gpios_chan = dma_claim_unused_channel(true);
	pio_sm_init(pio_gpios, sm_gpios, pio_gpios_offset, NULL);

	/**
	 * @brief Construct a new dma channel and pio abort object
	 *
	 */
	dma_channel_abort(dma_gpios_chan);
	pio_sm_set_enabled(pio_gpios, sm_gpios, false);
	pio_sm_clear_fifos(pio_gpios, sm_gpios);

	pio_sm_config c = bring_up_gpios_program_get_default_config(pio_gpios_offset);

	sm_config_set_in_pins(&c, GPIOS_START_PIN);
	sm_config_set_wrap(&c, pio_gpios_offset,
			   pio_gpios_offset + bring_up_gpios_program.length - 1);

	float div = (float)clk_sys / 1e6; // 1 MHz
	sm_config_set_clkdiv(&c, div);

	sm_config_set_in_shift(&c, false, true, 16);

	return;
}

void ana_gpios_get_data()
{
    dma_channel_start(dma_gpios_chan);
    pio_sm_set_enabled(pio_gpios, sm_gpios, true);

    dma_channel_wait_for_finish_blocking(dma_gpios_chan);
    pio_sm_set_enabled(pio_gpios, sm_gpios, false);
}

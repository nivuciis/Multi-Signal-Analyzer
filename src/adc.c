/*******************************************************************
 * @file adc.c
 *
 * @brief ADC implementation — DMA-based round-robin capture
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.2
 * @date 23/03/2026
 *
 * @copyright Copyright (c) 2026
 *
 * Capture path (ana_adc_capture_dma):
 *
 *   The ADC is configured in round-robin mode so it automatically cycles
 *   through the enabled channels after each conversion.  A single DMA
 *   channel reads the ADC FIFO using the ADC DREQ, writing interleaved
 *   12-bit samples into adc_raw_dma_buf.
 *
 *   After the DMA completes the raw buffer is demultiplexed: each
 *   channel's samples are extracted and converted to millivolts using
 *   the voltage-divider-aware conversion factor, then stored in the
 *   per-channel float buffers.
 *
 *   This avoids:
 *     - Changing adc_select_input() while the ADC is running (Bug A)
 *     - FIFO overflow from free-running mode + slow software loop (Bug B)
 *     - Capturing channels that are not enabled (Bug C)
 *     - PulseView timeouts from a slow blocking software loop (Bug D)
 *
 *******************************************************************/
#include "adc.h"
#include "log.h"

#include <stdint.h>
#include <hardware/adc.h>
#include <hardware/dma.h>

/*
 * ADC GPIO → hardware channel mapping for this board (RP2350B extended GPIO):
 *   GPIO 45 (CHANNEL_3 / sigrok ch 2) → ADC hw ch 5
 *   GPIO 46 (CHANNEL_2 / sigrok ch 1) → ADC hw ch 6
 *   GPIO 47 (CHANNEL_1 / sigrok ch 0) → ADC hw ch 7
 */
#define ADC_GPIO_TO_HW_CH(gpio) ((gpio) - 40u)

static const uint8_t SIGROK_CH_TO_GPIO[ADC_NUM_CHANNELS] = {
	PICO_DEFAULT_ADC_CHANNEL_1, /* sigrok ch 0 */
	PICO_DEFAULT_ADC_CHANNEL_2, /* sigrok ch 1 */
	PICO_DEFAULT_ADC_CHANNEL_3, /* sigrok ch 2 */
};

#define ADC_MV_PER_LSB  (3300.0f / 4096.0f)  /* 12-bit ADC, 0–3300mV range */

#define ADC_BUF_SIZE 1024

static uint16_t adc_buf_raw[ADC_NUM_CHANNELS][ADC_BUF_SIZE];

static uint16_t adc_dma_scratch[ADC_BUF_SIZE * ADC_NUM_CHANNELS];

struct ana_adc_module ana_adc = {
	.module = {
		.name      = "ADC",
		.pin_base  = PICO_DEFAULT_ADC_PIN_BASE,
		.pin_count = PICO_DEFAULT_ADC_PIN_COUNT,
		.mask      = 0x0000,
	},
	.clkdiv  = 0.0f,
	.dma_chan = -1,
	.raw     = {
		adc_buf_raw[0],
		adc_buf_raw[1],
		adc_buf_raw[2],
	},
};

void ana_adc_init(void)
{
	adc_init();

	for (int i = 0; i < ADC_NUM_CHANNELS; i++) {
		adc_gpio_init(SIGROK_CH_TO_GPIO[i]);
	}

	adc_run(false);
	ana_adc.dma_chan = dma_claim_unused_channel(true);
	log_debug(ana_adc.module.name, "DMA channel: %d", ana_adc.dma_chan);
}

void ana_adc_set_clkdiv(float clkdiv)
{
	ana_adc.clkdiv = clkdiv;
	adc_set_clkdiv(clkdiv);
}

void ana_adc_set_rate(uint32_t sample_rate_hz)
{
	if (sample_rate_hz == 0) {
		return;
	}

	/*
	 * The ADC samples off the 48 MHz clock, one conversion every
	 * (clkdiv + 1) cycles.  The minimum usable divisor is 95 (→ 500 kSps),
	 * which is the analyzer's hard analog limit (see AnalyzerDetails.md).
	 * Without this the ADC free-runs at full speed regardless of the
	 * requested sample rate, so analog samples land at the wrong spacing.
	 */
	float clkdiv = (48000000.0f / (float)sample_rate_hz) - 1.0f;
	if (clkdiv < 95.0f) {
		clkdiv = 95.0f;
	}

	ana_adc_set_clkdiv(clkdiv);
}

float ana_adc_read(uint8_t channel)
{
	adc_run(false);
	adc_select_input(ADC_GPIO_TO_HW_CH(channel));
	return (float)(adc_read());
}

bool ana_adc_capture_dma(uint32_t samples, uint8_t analog_mask)
{
	if (samples == 0 || analog_mask == 0) {
		return true;
	}
	if (samples > ADC_BUF_SIZE) {
		samples = ADC_BUF_SIZE;
	}


	uint32_t rr_hw_mask = 0;
	int      ch_order[ADC_NUM_CHANNELS];
	int      active_cnt = 0;

	for (int bit = 0; bit < ADC_NUM_CHANNELS; bit++) {
		if (analog_mask & (1u << bit)) {
			rr_hw_mask |= (1u << ADC_GPIO_TO_HW_CH(SIGROK_CH_TO_GPIO[bit]));
		}
	}

	for (int hw = 0; hw <= 7; hw++) {
		if (rr_hw_mask & (1u << hw)) {
			ch_order[active_cnt] = hw;
			active_cnt++;
		}
	}
	if (active_cnt == 0) {
		return true;
	}

	uint32_t total_xfers = samples * (uint32_t)active_cnt;

	adc_fifo_drain();
	/* err_in_fifo=true: a FIFO overrun sets bit 15 of the stored sample,
	 * so a high-rate overflow is detectable instead of silently corrupt.
	 * Limited to 150KHz with 3 analog channels */
	adc_fifo_setup(true, true, 1, true, false);
	adc_set_round_robin(rr_hw_mask);
	adc_select_input(ch_order[0]);

	dma_channel_config cfg = dma_channel_get_default_config(ana_adc.dma_chan);
	channel_config_set_read_increment(&cfg,  false);
	channel_config_set_write_increment(&cfg, true);
	channel_config_set_dreq(&cfg, DREQ_ADC);
	channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);

	dma_channel_configure(ana_adc.dma_chan, &cfg,
			      adc_dma_scratch, &adc_hw->fifo, total_xfers, true);

	adc_run(true);
	dma_channel_wait_for_finish_blocking(ana_adc.dma_chan);
	adc_run(false);
	adc_fifo_drain();


	bool overflow = false;

	for (uint32_t s = 0; s < samples; s++) {
		for (int c = 0; c < active_cnt; c++) {
			uint16_t raw  = adc_dma_scratch[s * (uint32_t)active_cnt + (uint32_t)c];
			int      hwch = ch_order[c];

			if (raw & 0x8000u) {
				overflow = true;
			}

			/* Map hw channel back to sigrok channel index */
			for (int bit = 0; bit < ADC_NUM_CHANNELS; bit++) {
				if ((analog_mask & (1u << bit)) &&
				    ADC_GPIO_TO_HW_CH(SIGROK_CH_TO_GPIO[bit]) == hwch) {
					ana_adc.raw[bit][s] = raw & 0x0FFFu;
					break;
				}
			}
		}
	}

	if (overflow) {
		log_warn(ana_adc.module.name, "ADC FIFO overflow at rate too high");
	}

	log_debug(ana_adc.module.name,
		  "Captured %lu samples x %d ch, rr_mask=0x%02X",
		  (unsigned long)samples, active_cnt, (unsigned)rr_hw_mask);

	return !overflow;
}

struct ana_adc_module *ana_adc_get_module(void)
{
	return &ana_adc;
}
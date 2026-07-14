/*******************************************************************
 * @file adc.c
 *
 * @brief ADC implementation — CPU-driven round-robin capture
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.3
 * @date 23/03/2026
 *
 * @copyright Copyright (c) 2026
 *
 * Capture path:
 *
 *   The ADC is configured in round-robin mode so it automatically cycles
 *   through the enabled channels after each conversion, pushing 12-bit
 *   samples into its 8-entry FIFO.  The CPU drains the FIFO into
 *   adc_scratch via ana_adc_capture_service(), which the digital CPU
 *   sampler calls from its pacing idle loop so the FIFO never overruns
 *   while a capture is in flight.  ana_adc_capture_finish() drains the
 *   remainder, then demultiplexes the interleaved scratch buffer into the
 *   per-channel raw buffers.
 *
 *   err_in_fifo is kept enabled: if the CPU ever falls behind, bit 15 of
 *   the stored sample flags the overrun instead of silently corrupting
 *   the stream.
 *
 *******************************************************************/
#include "adc.h"
#include "log.h"
#include "usb_util.h"

#include <stdint.h>
#include <hardware/adc.h>

/*
 * ADC GPIO → hardware channel mapping for this board (RP2350B extended GPIO):
 *   GPIO 45 (CHANNEL_3 / sigrok ch 2) → ADC hw ch 5
 *   GPIO 46 (CHANNEL_2 / sigrok ch 1) → ADC hw ch 6
 *   GPIO 47 (CHANNEL_1 / sigrok ch 0) → ADC hw ch 7
 */
#define ADC_GPIO_TO_HW_CH(gpio) ((gpio) - 40u)

#define ADC_BUF_SIZE 1024

#define ADC_MIN_CLKDIV 95.0f

static const uint8_t SIGROK_CH_TO_GPIO[ADC_NUM_CHANNELS] = {
	PICO_DEFAULT_ADC_CHANNEL_1, /* sigrok ch 0 */
	PICO_DEFAULT_ADC_CHANNEL_2, /* sigrok ch 1 */
	PICO_DEFAULT_ADC_CHANNEL_3, /* sigrok ch 2 */
};


static uint16_t adc_buf_raw[ADC_NUM_CHANNELS][ADC_BUF_SIZE];

static uint16_t adc_scratch[ADC_BUF_SIZE * ADC_NUM_CHANNELS];

struct ana_adc_module ana_adc = {
	.module = {
		.name      = "ADC",
		.pin_base  = PICO_DEFAULT_ADC_PIN_BASE,
		.pin_count = PICO_DEFAULT_ADC_PIN_COUNT,
		.mask      = 0x0000,
	},
	.clkdiv  = 0.0f,
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
	if (clkdiv < ADC_MIN_CLKDIV) {
		clkdiv = ADC_MIN_CLKDIV;
	}

	ana_adc_set_clkdiv(clkdiv);
}

float ana_adc_read(uint8_t channel)
{
	adc_run(false);
	adc_select_input(ADC_GPIO_TO_HW_CH(channel));
	return (float)(adc_read());
}

/* In-flight capture state shared between start/kick/service/finish/abort. */
static struct {
	uint32_t samples;
	uint32_t total_xfers;
	uint32_t written;
	uint8_t  analog_mask;
	int      ch_order[ADC_NUM_CHANNELS];
	int      active_cnt;
	volatile bool armed;   /**< Configured, waiting for the kick */
	volatile bool running; /**< Conversions started */
} adc_capture;

bool ana_adc_capture_start(uint32_t samples, uint8_t analog_mask)
{
	adc_capture.armed = false;
	adc_capture.running = false;

	if (samples == 0 || analog_mask == 0) {
		return true;
	}
	if (samples > ADC_BUF_SIZE) {
		samples = ADC_BUF_SIZE;
	}

	uint32_t rr_hw_mask = 0;

	adc_capture.active_cnt = 0;

	for (int bit = 0; bit < ADC_NUM_CHANNELS; bit++) {
		if (analog_mask & (1u << bit)) {
			rr_hw_mask |= (1u << ADC_GPIO_TO_HW_CH(SIGROK_CH_TO_GPIO[bit]));
		}
	}

	for (int hw = 0; hw <= 7; hw++) {
		if (rr_hw_mask & (1u << hw)) {
			adc_capture.ch_order[adc_capture.active_cnt] = hw;
			adc_capture.active_cnt++;
		}
	}
	if (adc_capture.active_cnt == 0) {
		return true;
	}

	adc_capture.samples = samples;
	adc_capture.analog_mask = analog_mask;
	adc_capture.total_xfers = samples * (uint32_t)adc_capture.active_cnt;
	adc_capture.written = 0;

	adc_fifo_drain();
	/* err_in_fifo=true: bit 15 of each stored sample flags a conversion
	 * error. FIFO overrun is separate — the sticky FCS.OVER flag, cleared
	 * here and checked in ana_adc_capture_finish(): an overrun drops
	 * samples, which shifts the round-robin interleave and lands every
	 * later sample on the wrong channel. */
	hw_set_bits(&adc_hw->fcs, ADC_FCS_OVER_BITS | ADC_FCS_UNDER_BITS);
	adc_fifo_setup(true, false, 1, true, false);
	adc_set_round_robin(rr_hw_mask);
	adc_select_input(adc_capture.ch_order[0]);

	/* Do not start converting yet: the CPU is about to spend an unbounded
	 * stretch sending the previous chunk over USB, and the 8-entry FIFO
	 * would overrun (and slip the channel phase) before the sampler loop
	 * begins draining it. ana_adc_capture_kick() starts the ADC at the
	 * same instant the digital sampling loop opens. */
	adc_capture.armed = true;

	return true;
}

void ana_adc_capture_kick(void)
{
	if (!adc_capture.armed || adc_capture.running) {
		return;
	}

	adc_run(true);
	adc_capture.running = true;
}

void ana_adc_capture_service(void)
{
	if (!adc_capture.running) {
		return;
	}

	while (adc_capture.written < adc_capture.total_xfers && !adc_fifo_is_empty()) {
		adc_scratch[adc_capture.written] = adc_fifo_get();
		adc_capture.written++;
	}

	if (adc_capture.written >= adc_capture.total_xfers) {
		adc_run(false);
	}
}

bool ana_adc_capture_finish(void)
{
	if (!adc_capture.armed) {
		return true;
	}

	/* Safety net: if the digital sampler never ran (and so never kicked
	 * the ADC), start it now so the drain below can complete. */
	ana_adc_capture_kick();

	while (adc_capture.written < adc_capture.total_xfers) {
		ana_adc_capture_service();

		if (!ana_usb_is_connected() || ana_usb_abort_requested()) {
			ana_adc_capture_abort();
			return false;
		}
	}

	adc_capture.armed = false;
	adc_capture.running = false;
	adc_run(false);

	/* A FIFO overrun dropped samples: the round-robin interleave slipped
	 * and the demux below would assign samples to the wrong channels. */
	bool overflow = (adc_hw->fcs & ADC_FCS_OVER_BITS) != 0u;

	adc_fifo_drain();

	for (uint32_t s = 0; s < adc_capture.samples; s++) {
		for (int c = 0; c < adc_capture.active_cnt; c++) {
			uint16_t raw  = adc_scratch[s * (uint32_t)adc_capture.active_cnt +
						    (uint32_t)c];
			int      hwch = adc_capture.ch_order[c];

			if (raw & 0x8000u) {
				overflow = true;
			}

			/* Map hw channel back to sigrok channel index */
			for (int bit = 0; bit < ADC_NUM_CHANNELS; bit++) {
				if ((adc_capture.analog_mask & (1u << bit)) &&
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
		  "Captured %lu samples x %d ch",
		  (unsigned long)adc_capture.samples, adc_capture.active_cnt);

	return !overflow;
}

void ana_adc_capture_abort(void)
{
	if (!adc_capture.armed && !adc_capture.running) {
		return;
	}
	adc_capture.armed = false;
	adc_capture.running = false;

	adc_run(false);
	adc_fifo_drain();
}

struct ana_adc_module *ana_adc_get_module(void)
{
	return &ana_adc;
}

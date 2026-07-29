/*******************************************************************
 * @file adc.h
 *
 * @brief ADC configuration and control interface for analog channels
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.2
 * @date 23/03/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/
#ifndef ADC_H
#define ADC_H

#include "handles/sigrok_handler.h"
#include "module.h"

#include <stdbool.h>
#include <stdint.h>

#include <hardware/adc.h>
#include <hardware/dma.h>

/**
 * @brief Raw 12-bit ADC buffers, one per sigrok analog channel (index 0–2).
 *
 * Sized to ADC_BUF_SIZE (1024) — one capture chunk, matching the digital
 * chunk size. Indexed as: raw[sigrok_ch][sample_index].
 */
#define ADC_NUM_CHANNELS 3

/**
 * @brief Configuration structure for the ADC module.
 */
struct ana_adc_module {
	struct ana_module_config const module;
	float clkdiv;
	int dma_chan;
	uint16_t *raw[ADC_NUM_CHANNELS]; /**< raw[ch][sample] — 12-bit values */
};

/**
 * @brief Initialize the ADC peripherals.
 */
void ana_adc_init(void);

/**
 * @brief Set the ADC free-running clock divider.
 */
void ana_adc_set_clkdiv(float clkdiv);

/**
 * @brief Set the ADC sample rate, deriving and clamping the clock divider.
 *
 * Keeps the analog conversion rate aligned with the requested capture rate
 * (clamped to the 500 kSps hard limit). Without this the ADC free-runs and
 * analog samples are time-distorted relative to the digital stream.
 *
 * @param sample_rate_hz Requested per-channel sample rate in Hz.
 */
void ana_adc_set_rate(uint32_t sample_rate_hz);

/**
 * @brief Single-shot read of one channel.
 *
 * @param channel Board GPIO number (e.g. PICO_DEFAULT_ADC_CHANNEL_1).
 * @return float  Reading in millivolts.
 */
float ana_adc_read(uint8_t channel);

/**
 * @brief Arm and start a DMA round-robin capture for all enabled channels.
 *
 * Non-blocking: configures the FIFO/round-robin, arms the DMA and starts the
 * ADC, then returns. Call alongside the digital capture start so the analog
 * window opens at the same instant as the digital one.
 *
 * @param samples     Samples per enabled channel (capped to ADC_BUF_SIZE).
 * @param analog_mask Sigrok bitmask: bit 0 = ch0 (GPIO 47), bit 1 = ch1 (GPIO 46),
 *                    bit 2 = ch2 (GPIO 45).
 * @return bool       true if the capture was started (or nothing to do).
 */
bool ana_adc_capture_start(uint32_t samples, uint8_t analog_mask);

/**
 * @brief Wait for the capture started by ana_adc_capture_start() and demux it.
 *
 * Blocks until the DMA completes, then fills raw[ch][0..samples-1] with
 * 12-bit values. No-op if no capture is in flight.
 *
 * @return bool true on success, false if a FIFO overflow was detected.
 */
bool ana_adc_capture_finish(void);

/**
 * @brief Abort an in-flight ADC capture (stop ADC, abort DMA, drain FIFO).
 */
void ana_adc_capture_abort(void);

/**
 * @brief Pack one raw ADC sample into a sigrok analog byte.
 *
 * Format: 0x80 | (raw12 >> 5)  — top 7 bits, high bit always set.
 *
 * @param sigrok_ch  Sigrok channel index (0–2).
 * @param sample_idx Sample index within the capture.
 * @return uint8_t   Byte ready to write into the TX stream.
 */
static inline uint8_t ana_adc_sigrok_byte(uint8_t sigrok_ch, uint32_t sample_idx,
					  struct ana_adc_module *adc)
{
	return (uint8_t)(0x80u | ((adc->raw[sigrok_ch][sample_idx] >> 5) & 0x7Fu));
}

/**
 * @brief Return a pointer to the ADC module state.
 */
struct ana_adc_module *ana_adc_get_module(void);

#endif /* ADC_H */

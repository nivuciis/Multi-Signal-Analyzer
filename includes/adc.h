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

#include "module.h"
#include "handles/sigrok_handler.h"

#include <stdbool.h>
#include <stdint.h>
#include <hardware/adc.h>

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
	float    clkdiv;
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
 * @brief Arm and start a CPU round-robin capture for all enabled channels.
 *
 * Non-blocking: configures the FIFO/round-robin and arms the capture, then
 * returns. Conversions only start at ana_adc_capture_kick(), issued by the
 * digital CPU sampler when its loop opens, so the analog window aligns with
 * the digital one and the FIFO never overruns while the CPU is busy
 * elsewhere. While the capture is in flight, ana_adc_capture_service() must
 * be called often enough to keep the 8-entry FIFO from overrunning (the
 * digital CPU sampler does this from its pacing idle loop).
 *
 * @param samples     Samples per enabled channel (capped to ADC_BUF_SIZE).
 * @param analog_mask Sigrok bitmask: bit 0 = ch0 (GPIO 47), bit 1 = ch1 (GPIO 46),
 *                    bit 2 = ch2 (GPIO 45).
 * @return bool       true if the capture was started (or nothing to do).
 */
bool ana_adc_capture_start(uint32_t samples, uint8_t analog_mask);

/**
 * @brief Start the conversions armed by ana_adc_capture_start().
 *
 * Called by the digital CPU sampler at the instant its sampling loop opens,
 * so the analog and digital windows align and the FIFO is drained from the
 * first conversion on. No-op if not armed or already running.
 */
void ana_adc_capture_kick(void);

/**
 * @brief Drain any pending ADC FIFO entries into the capture buffer.
 *
 * Non-blocking; no-op when no capture is in flight. Call from tight loops
 * that run while an analog capture is active.
 */
void ana_adc_capture_service(void);

/**
 * @brief Wait for the capture started by ana_adc_capture_start() and demux it.
 *
 * Drains the remaining samples with the CPU, then fills raw[ch][0..samples-1]
 * with 12-bit values. No-op if no capture is in flight.
 *
 * @return bool true on success, false if a FIFO overflow was detected or the
 *              capture was aborted.
 */
bool ana_adc_capture_finish(void);

/**
 * @brief Abort an in-flight ADC capture (stop ADC, drain FIFO).
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
static inline uint8_t ana_adc_sigrok_byte(uint8_t sigrok_ch, uint32_t sample_idx, struct ana_adc_module *adc)
{
	return (uint8_t)(0x80u | ((adc->raw[sigrok_ch][sample_idx] >> 5) & 0x7Fu));
}

/**
 * @brief Return a pointer to the ADC module state.
 */
struct ana_adc_module *ana_adc_get_module(void);

#endif /* ADC_H */
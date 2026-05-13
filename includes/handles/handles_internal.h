/*******************************************************************
 * @file handles_internal.h
 *
 * @brief Internal header shared between sigrok_handler.c and handle files.
 *
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 12/05/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#ifndef HANDLES_INTERNAL_H
#define HANDLES_INTERNAL_H

#include "handles/sigrok_handler.h"
#include "log.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * @brief Size of the transmit buffer for Sigrok responses and captures.
 *
 */
#define TX_BUF_SIZE 4096U

/**
 * @brief Minimum sample rate for Sigrok
 */
#define SIGROK_SAMPLE_RATE_MIN 5000U

/**
 * @brief Maximum sample rate for Sigrok
 */
#define SIGROK_SAMPLE_RATE_MAX 120000000U

/**
 * @brief Maximum number of samples for Sigrok
 */
#define SIGROK_SAMPLE_LIMIT_MAX 1000000U

/**
 * @brief Command buffer size for building incoming commands before processing.
 *
 */
#define ANA_TX_BUF_CMD_SIZE 32

/**
 * @brief Response buffer size for building outgoing responses to the host.
 *
 */
#define ANA_TX_BUF_RESPONSE_SIZE 64

/**
 * @brief Internal function to run a capture, used by both fixed and continuous capture handlers.
 *
 */
#if PICO_DEFAULT_ADC_VOLTAGE_DIVIDER
#define ADC_MV_FULL_SCALE   13200.0
#define SIGROK_ANALOG_SCALE "103125x0"
#else
#define ADC_MV_FULL_SCALE   3300.0
#define SIGROK_ANALOG_SCALE "25700x0"
#endif

/**
 * @brief Default digital channel masks for Sigrok.
 *
 */
#define DIGITAL_MASK_DEFAULT 0x0FFF

/**
 * @brief Default analog channel mask for Sigrok.
 *
 */
#define ANALOG_MASK_DEFAULT 0x00

/**
 * @brief Internal structure representing the state of the Sigrok handler.
 *
 */
struct SIGROK_HANDLER {
	struct pulseview_sample_config cfg; /**< Configuration for pulseview samples */
	struct {
		uint32_t bytes_per_dig_sample; /**< Number of bytes per digital sample */
		uint32_t active_analog_ch;     /**< Number of active analog channels */
		uint32_t bytes_per_sample;     /**< Total bytes per sample (digital + analog) */
		uint8_t buf[TX_BUF_SIZE]; /**< Buffer for building responses and capture data */
	} tx;
	uint32_t sample_rate;    /**< Sample rate in Hz */
	uint32_t num_samples;    /**< Number of samples to capture */
	uint16_t digital_mask;   /**< Bitmask of enabled digital channels */
	uint8_t analog_mask;     /**< Bitmask of enabled analog channels */
	uint8_t analog_channel;  /**< Currently selected analog channel for single-shot reads */
	uint16_t digital_channel; /**< Currently selected digital channel for single-shot reads */
	uint8_t digital_bits_per_transfer; /**< Number of digital bits per transfer (depends on
					      enabled channels) */
	int8_t cmd_str_index; /**< Index for building the command string from incoming bytes */
	int8_t cmd_str[ANA_TX_BUF_CMD_SIZE]; /**< Buffer for the incoming command string, built
						byte-by-byte */
	int8_t response[ANA_TX_BUF_RESPONSE_SIZE]; /**< Buffer for the response string to send back
						      to the host */
	char *end_ptr; /**< Pointer used for parsing numbers from the command string */
	struct sigrok_trigger trigger_config; /**< Current trigger configuration */
	bool last_was_cr; /**< Flag to track if the last received byte was a carriage return, used
			     for command parsing */
};

/**
 * @brief Send a response string back to the host over USB CDC.
 *
 * @param str Null-terminated string to send.
 */
void ana_send_response(const char *str);

/**
 * @brief Send raw bytes back to the host over USB CDC.
 *
 * @param continuous true if this is part of a continuous capture (for logging purposes).
 */
void run_capture(bool continuous);

/**
 * @brief Get a pointer to the global SIGROK_HANDLER instance.
 *
 * @return struct SIGROK_HANDLER* Pointer to the handler state.
 */
struct SIGROK_HANDLER *ana_sigrok_get_self(void);

#endif /* HANDLES_INTERNAL_H */

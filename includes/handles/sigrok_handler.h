/** -------------------------------------------------------------
 * @file sigrok_handler.h
 * @brief Sigrok protocol handler interface
 *
 * @note: All commands are terminated by CR or LF, except '*' which is
 * processed immediately without a terminator.
 *
 * Protocol reference: https://github.com/pico-coder/sigrok-pico
 *
 * Identification string format: "SRPICO,AxxxDyyy,vv"
 *   xxx = number of analog channels (e.g. 03)
 *   yyy = number of digital channels (e.g. 16)
 *   vv  = protocol version (02)
 *
 * Data stream format (high bit always set for sample bytes):
 *   - Digital bytes:  0x80–0xFF  (7 usable bits per byte)
 *   - RLE count byte: 0x30–0x7F  (high bit clear — marks a count, not data)
 *   - Abort marker:   "!!!"
 *   - Done marker:    "$<n>+"     (n = total bytes sent as ASCII decimal)
 *
 * @author   Vinicius Rafael Marques de Carvalho <vinicius.carvalho@edge.ufal.br>
 * @author   João Matheus Nascimento Dias <joao.dias@edge.ufal.br>
 * @version  0.2
 * @date     19/03/2026
 * @copyright Copyright (c) 2026
 *  ------------------------------------------------------------*/

#ifndef SIGROK_HANDLER_H
#define SIGROK_HANDLER_H

#include "board_def.h"

#include <stdint.h>

#define MAX_NUM_CHANNELS                                                                               \
PICO_DEFAULT_CHANNELS_PIN_COUNT + PICO_DEFAULT_CAN_PIN_COUNT +                             \
	PICO_DEFAULT_RS232_PIN_COUNT + PICO_DEFAULT_RS485_PIN_COUNT

/**
 * @brief Command list
 *
 */
enum SIGROK_PROTOCOL_COMMANDS {
	SIGROK_CMD_IDENTIFY = 'i',            /**< 'i' — Identity query. Responds with "SRPICO,AxxyDzz,02". */
	SIGROK_CMD_SET_SAMPLE_RATE = 'R',     /**< 'R<hz>' — Set sample rate in Hz. Range: 5000–120000000. */
	SIGROK_CMD_SET_SAMPLE_LIMIT = 'L',    /**< 'L<n>' — Set number of samples to capture (fixed mode). */
	SIGROK_CMD_GET_ANALOG_SCALE = 'a',    /**< 'a<ch>' — Query analog scale for channel ch (0–2). */
	SIGROK_CMD_SET_ANALOG_CHANNEL = 'A',  /**< 'A<en><ch>' — Enable (en=1) or disable (en=0) analog channel ch. */
	SIGROK_CMD_SET_DIGITAL_CHANNEL = 'D', /**< 'D<en><ch>' — Enable (en=1) or disable (en=0) digital channel ch (0-based). */
	SIGROK_CMD_FIXED_CAPTURE = 'F',       /**< 'F' — Fixed capture. No ACK; data + "$<n>+" terminates. */
	SIGROK_CMD_CONTINUOUS_CAPTURE = 'C',  /**< 'C' — Continuous capture (SW-triggered by driver). No ACK. */
	SIGROK_CMD_SET_PRETRIGGER = 'p',      /**< 'p<n>' — Pretrigger buffer depth hint. ACK with '*'. */
	SIGROK_CMD_SET_TRIGGER = 't',         /**< 't<type><idx>' — HW trigger. type: 0=low,1=high,2=rise,3=fall,4=edge. idx = ch+2. */
};

/**
 * @brief System status for capture and transmission state machine
 *
 */
enum ANA_SYSTEM_STATUS {
	ANA_SYSTEM_IDLE = 0X00,  /**< System is idle */
	ANA_SYSTEM_BUSY,         /**< System is busy */
	ANA_SYSTEM_STARTED,      /**< System has started */
	ANA_SYSTEM_SENDING,      /**< System is sending data */
	ANA_SYSTEM_DMA_DONE,     /**< DMA transfer is complete */
	ANA_SYSTEM_SAMPLES_SENT, /**< Samples have been sent */
	ANA_SYSTEM_ABORTED,      /**< Transfer was aborted */
};

/**
 * @brief Trigger types for the system
 *
 */
enum ana_trigger_type {
	ANA_TRIGGER_EDGE_RISE,  /**< Rising edge trigger */
	ANA_TRIGGER_EDGE_FALL,  /**< Falling edge trigger */
	ANA_TRIGGER_EDGE_BOTH,  /**< Both edges trigger */
	ANA_TRIGGER_LEVEL_LOW,  /**< Low level trigger */
	ANA_TRIGGER_LEVEL_HIGH, /**< High level trigger */
};

/**
 * @brief System trigger to channels
 *
 */
struct sigrok_trigger {
	uint16_t trigger_mask; /**< Bitmask of digital channels involved in the trigger condition */
	enum ana_trigger_type trigger_type[MAX_NUM_CHANNELS]; /**< Type of trigger */
};

/**
 * @brief Sample configuration from pulseview
 *
 */
struct pulseview_sample_config {
	uint32_t sample_rate_hz;
	uint32_t samples;
};

/**
 * @brief Initialize the Sigrok protocol handler.
 */
void ana_sigrok_handle_init(void);

/**
 * @brief Process one received byte from the Sigrok host.
 *
 * @param received_command Byte received from the host.
 */
void ana_sigrok_handle_process_byte(uint8_t received_command);

/**
 * @brief Take the configured sample parameters to configure the system.
 *
 * @return struct pulseview_sample_config Pointer to the current sample configuration structure
 */
struct pulseview_sample_config *ana_sigrok_get_sample_config(void);

/**
 * @brief Get the current trigger configuration.
 *
 * @return struct sigrok_trigger* Pointer to the trigger configuration
 */
struct sigrok_trigger *ana_sigrok_get_trigger(void);

#endif /* SIGROK_HANDLER_H */

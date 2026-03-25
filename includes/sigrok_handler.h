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

#include <stdint.h>

/**
 * @brief Maximum number of samples per capture, as limited by the protocol.
 * 
 */
#define SIGROK_SAMPLE_LIMIT_MAX 1024U  

/**
 * @brief Command list
 * 
 */
enum SIGROK_PROTOCOL_COMMANDS {
	SIGROK_CMD_IDENTIFY = 'i',           	/** 'i' — Identity query. Firmware responds with capability string. */
	SIGROK_CMD_SET_SAMPLE_RATE = 'R',    	/** 'R<hz>' — Set sample rate in Hz. Range: 5000–120000000. */
	SIGROK_CMD_SET_SAMPLE_LIMIT = 'L',   	/** 'L<n>' — Set number of samples to capture (fixed mode). */
	SIGROK_CMD_GET_ANALOG_SCALE = 'a',   	/** 'a<ch>' — Query analog scale for channel ch (0–2). */
	SIGROK_CMD_SET_ANALOG_CHANNEL = 'A', 	/** 'A<en><ch>' — Enable (en=1) or disable (en=0) analog channel ch. */
	SIGROK_CMD_SET_DIGITAL_CHANNEL = 'D',	/** 'D<en><ch>' — Enable (en=1) or disable (en=0) digital channel ch. */
	SIGROK_CMD_FIXED_CAPTURE = 'F', 	 	/** 'F' — Start fixed capture (exactly L samples). */
	SIGROK_CMD_CONTINUOUS_CAPTURE = 'C', 	/** 'C' — Start continuous capture (runs until host sends '*'). */
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
struct pulseview_sample_config* ana_sigrok_get_sample_config(void);

#endif /* SIGROK_HANDLER_H */

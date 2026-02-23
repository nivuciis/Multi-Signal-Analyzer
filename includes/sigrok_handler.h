/** -------------------------------------------------------------
 * @file sigrok_handler.h
 * @brief Sigrok protocol handler interface
 *
 * @author   Vinicius Rafael Marques de Carvalho <vinicius.carvalho@edge.ufal.br>
 * @version  0.1
 * @date     28/01/2026
 * @copyright Copyright (c) 2026
 *  ------------------------------------------------------------*/

#ifndef SIGROK_HANDLER_H
#define SIGROK_HANDLER_H

#include <stdint.h>

/**
 * @brief Enum for the supported Sigrok commands
 *
 */
enum SIGROK_PROTOCOL_COMMANDS {
	SIGROK_CMD_IDENTIFY = 'i',            /**< Identifier for the device to be recognized by Sigrok/PulseView */
	SIGROK_CMD_SET_SAMPLE_RATE = 'R',     /**< Command to set the sample rate */
	SIGROK_CMD_SET_SAMPLE_LIMIT = 'L',    /**< Command to set the sample limit */
	SIGROK_CMD_GET_ANALOG_SCALE = 'a',    /**< Command to get the analog scale */
	SIGROK_CMD_SET_ANALOG_CHANNEL = 'A',  /**< Command to cmd[Axyy]: (A)nalog x=1(enable)/x=0(disable) (yy) channels */
	SIGROK_CMD_SET_DIGITAL_CHANNEL = 'D', /**< Command to cmd[Dxyy]: (D)igital x=1(enable)/x=0(disable) (yy)channels */
	SIGROK_CMD_FIXED_CAPTURE = 'F',       /**< Command to start capture process */
};

/**
 * @brief Initialize the Sigrok protocol handler
 */
void ana_sigrok_handle_init(void);

/**
 * @brief Process the received messages from Sigrok/PulseView
 *
 * @param received_message The received command byte
 */
void ana_sigrok_handle_process_byte(uint8_t received_command);

#endif

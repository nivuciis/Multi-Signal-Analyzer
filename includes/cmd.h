/*******************************************************************
 * @file cmd.h
 *
 * @brief Commands definition to teste firmware features
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 08/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/
#ifndef CMD_H
#define CMD_H

/**
 * @brief Enumeration of command codes
 *
 */
enum ana_cmd_code {
	ANA_CMD_NONE = 0x00, /**< No command received */
	ANA_CMD_ADC,         /**< ADC command to test */
	ANA_CMD_CAN,         /**< CAN command to test */
	ANA_CMD_GPIOS,       /**< GPIOS command to test */
	ANA_CMD_LED,         /**< LED command to test */
	ANA_CMD_RS232,       /**< RS232 command to test */
	ANA_CMD_RS485,       /**< RS484 command to test */
	ANA_CMD_PWM,         /**< PWM command to test */
	_ANA_CMD_AMOUNT,     /**< Amount of commands available */
};

/**
 * @brief Process the received command
 *
 * @param cmd_code The command code to be processed
 */
void ana_cmd_process(enum ana_cmd_code cmd_code);

/**
 * @brief Print the command table to the console
 * 
 */
void ana_cmd_table(void);

#endif /* CMD_H */

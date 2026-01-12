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

enum ana_cmd_code {
	ANA_CMD_NONE = 0, /**< No command received */
    ANA_CMD_ADC,
    ANA_CMD_CAN,
    ANA_CMD_GPIOS,
    ANA_CMD_LED,
    ANA_CMD_RS232,
    ANA_CMD_RS484,
	_ANA_CMD_AMOUNT,   /**< Amount of commands available */
};

/**
 * @brief Process the received command
 * 
 * @param cmd_code The command code to be processed
 */
void ana_cmd_process(enum ana_cmd_code cmd_code);

#endif /* CMD_H */

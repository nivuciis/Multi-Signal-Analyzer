/*******************************************************************
 * @file rs485.h
 *
 * @brief Led control commands
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 08/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/
#ifndef RS485_H
#define RS485_H

/**
 * @brief GPIO pin for RS485 RX
 * 
 */
#define RS485_GPIO_PIN_RX 31

/**
 * @brief Initialize the RS485 system.
 * 
 */
void ana_rs485_init(void);

#endif /* RS485_H */
